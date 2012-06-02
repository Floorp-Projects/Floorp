/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Wrapper object for reflecting native xpcom objects into JavaScript. */

#include "xpcprivate.h"
#include "nsCRT.h"
#include "XPCWrapper.h"
#include "nsWrapperCacheInlines.h"
#include "XPCLog.h"
#include "nsINode.h"
#include "XPCQuickStubs.h"
#include "jsproxy.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "dombindings.h"

#include "nsContentUtils.h"

#include "mozilla/StandardInteger.h"
#include "mozilla/Util.h"

using namespace xpc;

bool
xpc_OkToHandOutWrapper(nsWrapperCache *cache)
{
    NS_ABORT_IF_FALSE(cache->GetWrapper(), "Must have wrapper");
    NS_ABORT_IF_FALSE(cache->IsDOMBinding() || IS_WN_WRAPPER(cache->GetWrapper()),
                      "Must have proxy or XPCWrappedNative wrapper");
    return cache->IsDOMBinding() ?
        mozilla::dom::binding::instanceIsProxy(cache->GetWrapper()) :
        !static_cast<XPCWrappedNative*>(xpc_GetJSPrivate(cache->GetWrapper()))->
            NeedsSOW();
}

/***************************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(XPCWrappedNative)

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(XPCWrappedNative)::Unlink(void *p)
{
    XPCWrappedNative *tmp = static_cast<XPCWrappedNative*>(p);
    tmp->ExpireWrapper();
    return NS_OK;
}

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(XPCWrappedNative)::Traverse(void *p,
                                                          nsCycleCollectionTraversalCallback &cb)
{
    XPCWrappedNative *tmp = static_cast<XPCWrappedNative*>(p);
    if (!tmp->IsValid())
        return NS_OK;

    if (NS_UNLIKELY(cb.WantDebugInfo())) {
        char name[72];
        XPCNativeScriptableInfo* si = tmp->GetScriptableInfo();
        if (si)
            JS_snprintf(name, sizeof(name), "XPCWrappedNative (%s)",
                        si->GetJSClass()->name);
        else
            JS_snprintf(name, sizeof(name), "XPCWrappedNative");

        cb.DescribeRefCountedNode(tmp->mRefCnt.get(),
                                  sizeof(XPCWrappedNative), name);
    } else {
        NS_IMPL_CYCLE_COLLECTION_DESCRIBE(XPCWrappedNative, tmp->mRefCnt.get())
    }

    if (tmp->mRefCnt.get() > 1) {

        // If our refcount is > 1, our reference to the flat JS object is
        // considered "strong", and we're going to traverse it.
        //
        // If our refcount is <= 1, our reference to the flat JS object is
        // considered "weak", and we're *not* going to traverse it.
        //
        // This reasoning is in line with the slightly confusing lifecycle rules
        // for XPCWrappedNatives, described in a larger comment below and also
        // on our wiki at http://wiki.mozilla.org/XPConnect_object_wrapping

        JSObject *obj = tmp->GetFlatJSObjectPreserveColor();
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mFlatJSObject");
        cb.NoteJSChild(obj);
    }

    if (tmp->MightHaveExpandoObject()) {
        XPCJSRuntime *rt = tmp->GetRuntime();
        XPCCompartmentSet &set = rt->GetCompartmentSet();
        for (XPCCompartmentRange r = set.all(); !r.empty(); r.popFront()) {
            CompartmentPrivate *priv = GetCompartmentPrivate(r.front());
            NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "XPCWrappedNative expando object");
            cb.NoteJSChild(priv->LookupExpandoObjectPreserveColor(tmp));
        }
    }

    // XPCWrappedNative keeps its native object alive.
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mIdentity");
    cb.NoteXPCOMChild(tmp->GetIdentityObject());

    tmp->NoteTearoffs(cb);

    return NS_OK;
}

void
XPCWrappedNative::NoteTearoffs(nsCycleCollectionTraversalCallback& cb)
{
    // Tearoffs hold their native object alive. If their JS object hasn't been
    // finalized yet we'll note the edge between the JS object and the native
    // (see nsXPConnect::Traverse), but if their JS object has been finalized
    // then the tearoff is only reachable through the XPCWrappedNative, so we
    // record an edge here.
    XPCWrappedNativeTearOffChunk* chunk;
    for (chunk = &mFirstChunk; chunk; chunk = chunk->mNextChunk) {
        XPCWrappedNativeTearOff* to = chunk->mTearOffs;
        for (int i = XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK-1; i >= 0; i--, to++) {
            JSObject* jso = to->GetJSObjectPreserveColor();
            if (!jso) {
                NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "tearoff's mNative");
                cb.NoteXPCOMChild(to->GetNative());
            }
        }
    }
}

#ifdef XPC_CHECK_CLASSINFO_CLAIMS
static void DEBUG_CheckClassInfoClaims(XPCWrappedNative* wrapper);
#else
#define DEBUG_CheckClassInfoClaims(wrapper) ((void)0)
#endif

#ifdef XPC_TRACK_WRAPPER_STATS
static int DEBUG_TotalWrappedNativeCount;
static int DEBUG_TotalLiveWrappedNativeCount;
static int DEBUG_TotalMaxWrappedNativeCount;
static int DEBUG_WrappedNativeWithProtoCount;
static int DEBUG_LiveWrappedNativeWithProtoCount;
static int DEBUG_MaxWrappedNativeWithProtoCount;
static int DEBUG_WrappedNativeNoProtoCount;
static int DEBUG_LiveWrappedNativeNoProtoCount;
static int DEBUG_MaxWrappedNativeNoProtoCount;
static int DEBUG_WrappedNativeTotalCalls;
static int DEBUG_WrappedNativeMethodCalls;
static int DEBUG_WrappedNativeGetterCalls;
static int DEBUG_WrappedNativeSetterCalls;
#define DEBUG_CHUNKS_TO_COUNT 4
static int DEBUG_WrappedNativeTearOffChunkCounts[DEBUG_CHUNKS_TO_COUNT+1];
static bool    DEBUG_DumpedWrapperStats;
#endif

#ifdef DEBUG
static void DEBUG_TrackNewWrapper(XPCWrappedNative* wrapper)
{
#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    if (wrapper->GetRuntime())
        wrapper->GetRuntime()->DEBUG_AddWrappedNative(wrapper);
    else
        NS_ERROR("failed to add wrapper");
#endif
#ifdef XPC_TRACK_WRAPPER_STATS
    DEBUG_TotalWrappedNativeCount++;
    DEBUG_TotalLiveWrappedNativeCount++;
    if (DEBUG_TotalMaxWrappedNativeCount < DEBUG_TotalLiveWrappedNativeCount)
        DEBUG_TotalMaxWrappedNativeCount = DEBUG_TotalLiveWrappedNativeCount;

    if (wrapper->HasProto()) {
        DEBUG_WrappedNativeWithProtoCount++;
        DEBUG_LiveWrappedNativeWithProtoCount++;
        if (DEBUG_MaxWrappedNativeWithProtoCount < DEBUG_LiveWrappedNativeWithProtoCount)
            DEBUG_MaxWrappedNativeWithProtoCount = DEBUG_LiveWrappedNativeWithProtoCount;
    } else {
        DEBUG_WrappedNativeNoProtoCount++;
        DEBUG_LiveWrappedNativeNoProtoCount++;
        if (DEBUG_MaxWrappedNativeNoProtoCount < DEBUG_LiveWrappedNativeNoProtoCount)
            DEBUG_MaxWrappedNativeNoProtoCount = DEBUG_LiveWrappedNativeNoProtoCount;
    }
#endif
}

static void DEBUG_TrackDeleteWrapper(XPCWrappedNative* wrapper)
{
#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
    nsXPConnect::GetRuntimeInstance()->DEBUG_RemoveWrappedNative(wrapper);
#endif
#ifdef XPC_TRACK_WRAPPER_STATS
    DEBUG_TotalLiveWrappedNativeCount--;
    if (wrapper->HasProto())
        DEBUG_LiveWrappedNativeWithProtoCount--;
    else
        DEBUG_LiveWrappedNativeNoProtoCount--;

    int extraChunkCount = wrapper->DEBUG_CountOfTearoffChunks() - 1;
    if (extraChunkCount > DEBUG_CHUNKS_TO_COUNT)
        extraChunkCount = DEBUG_CHUNKS_TO_COUNT;
    DEBUG_WrappedNativeTearOffChunkCounts[extraChunkCount]++;
#endif
}
static void DEBUG_TrackWrapperCall(XPCWrappedNative* wrapper,
                                   XPCWrappedNative::CallMode mode)
{
#ifdef XPC_TRACK_WRAPPER_STATS
    DEBUG_WrappedNativeTotalCalls++;
    switch (mode) {
        case XPCWrappedNative::CALL_METHOD:
            DEBUG_WrappedNativeMethodCalls++;
            break;
        case XPCWrappedNative::CALL_GETTER:
            DEBUG_WrappedNativeGetterCalls++;
            break;
        case XPCWrappedNative::CALL_SETTER:
            DEBUG_WrappedNativeSetterCalls++;
            break;
        default:
            NS_ERROR("bad value");
    }
#endif
}

static void DEBUG_TrackShutdownWrapper(XPCWrappedNative* wrapper)
{
#ifdef XPC_TRACK_WRAPPER_STATS
    if (!DEBUG_DumpedWrapperStats) {
        DEBUG_DumpedWrapperStats = true;
        printf("%d WrappedNatives were constructed. "
               "(%d w/ protos, %d w/o)\n",
               DEBUG_TotalWrappedNativeCount,
               DEBUG_WrappedNativeWithProtoCount,
               DEBUG_WrappedNativeNoProtoCount);

        printf("%d WrappedNatives max alive at one time. "
               "(%d w/ protos, %d w/o)\n",
               DEBUG_TotalMaxWrappedNativeCount,
               DEBUG_MaxWrappedNativeWithProtoCount,
               DEBUG_MaxWrappedNativeNoProtoCount);

        printf("%d WrappedNatives alive now. "
               "(%d w/ protos, %d w/o)\n",
               DEBUG_TotalLiveWrappedNativeCount,
               DEBUG_LiveWrappedNativeWithProtoCount,
               DEBUG_LiveWrappedNativeNoProtoCount);

        printf("%d calls to WrappedNatives. "
               "(%d methods, %d getters, %d setters)\n",
               DEBUG_WrappedNativeTotalCalls,
               DEBUG_WrappedNativeMethodCalls,
               DEBUG_WrappedNativeGetterCalls,
               DEBUG_WrappedNativeSetterCalls);

        printf("(wrappers / tearoffs): (");
        int i;
        for (i = 0; i < DEBUG_CHUNKS_TO_COUNT; i++) {
            printf("%d / %d, ",
                   DEBUG_WrappedNativeTearOffChunkCounts[i],
                   (i+1) * XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK);
        }
        printf("%d / more)\n", DEBUG_WrappedNativeTearOffChunkCounts[i]);
    }
#endif
}
#else
#define DEBUG_TrackNewWrapper(wrapper) ((void)0)
#define DEBUG_TrackDeleteWrapper(wrapper) ((void)0)
#define DEBUG_TrackWrapperCall(wrapper, mode) ((void)0)
#define DEBUG_TrackShutdownWrapper(wrapper) ((void)0)
#endif

/***************************************************************************/
static nsresult
FinishCreate(XPCCallContext& ccx,
             XPCWrappedNativeScope* Scope,
             XPCNativeInterface* Interface,
             nsWrapperCache *cache,
             XPCWrappedNative* inWrapper,
             XPCWrappedNative** resultWrapper);

// static
//
// This method handles the special case of wrapping a new global object.
//
// The normal code path for wrapping natives goes through
// XPCConvert::NativeInterface2JSObject, XPCWrappedNative::GetNewOrUsed,
// and finally into XPCWrappedNative::Init. Unfortunately, this path assumes
// very early on that we have an XPCWrappedNativeScope and corresponding global
// JS object, which are the very things we need to create here. So we special-
// case the logic and do some things in a different order.
nsresult
XPCWrappedNative::WrapNewGlobal(XPCCallContext &ccx, xpcObjectHelper &nativeHelper,
                                nsIPrincipal *principal, bool initStandardClasses,
                                XPCWrappedNative **wrappedGlobal)
{
    bool success;
    nsresult rv;
    nsISupports *identity = nativeHelper.GetCanonical();

    // The object should specify that it's meant to be global.
    MOZ_ASSERT(nativeHelper.GetScriptableFlags() & nsIXPCScriptable::IS_GLOBAL_OBJECT);

    // We shouldn't be reusing globals.
    MOZ_ASSERT(!nativeHelper.GetWrapperCache() ||
               !nativeHelper.GetWrapperCache()->GetWrapperPreserveColor());

    // Put together the ScriptableCreateInfo...
    XPCNativeScriptableCreateInfo sciProto;
    XPCNativeScriptableCreateInfo sciMaybe;
    const XPCNativeScriptableCreateInfo& sciWrapper =
        GatherScriptableCreateInfo(identity, nativeHelper.GetClassInfo(),
                                   sciProto, sciMaybe);

    // ...and then ScriptableInfo. We need all this stuff now because it's going
    // to tell us the JSClass of the object we're going to create.
    AutoMarkingNativeScriptableInfoPtr
        si(ccx, XPCNativeScriptableInfo::Construct(ccx, &sciWrapper));
    MOZ_ASSERT(si.get());

    // Finally, we get to the JSClass.
    JSClass *clasp = si->GetJSClass();
    MOZ_ASSERT(clasp->flags & JSCLASS_IS_GLOBAL);

    // Create the global.
    JSObject *global;
    JSCompartment *compartment;
    rv = xpc_CreateGlobalObject(ccx, clasp, principal, nsnull, false,
                                &global, &compartment);
    NS_ENSURE_SUCCESS(rv, rv);

    // Immediately enter the global's compartment, so that everything else we
    // create ends up there.
    JSAutoEnterCompartment ac;
    success = ac.enter(ccx, global);
    MOZ_ASSERT(success);

    // If requested, immediately initialize the standard classes on the global.
    // We need to do this before creating a scope, because
    // XPCWrappedNativeScope::SetGlobal resolves |Object| via
    // JS_ResolveStandardClass. JS_InitStandardClasses asserts if any of the
    // standard classes are already initialized, so this is a problem.
    if (initStandardClasses && ! JS_InitStandardClasses(ccx, global))
        return NS_ERROR_FAILURE;

    // Create a scope, but don't do any extra stuff like initializing |Components|.
    // All of that stuff happens in the caller.
    XPCWrappedNativeScope *scope = XPCWrappedNativeScope::GetNewOrUsed(ccx, global, identity);
    MOZ_ASSERT(scope);

    // Make a proto.
    XPCWrappedNativeProto *proto =
        XPCWrappedNativeProto::GetNewOrUsed(ccx, scope, nativeHelper.GetClassInfo(), &sciProto,
                                            UNKNOWN_OFFSETS, /* callPostCreatePrototype = */ false);
    if (!proto)
        return NS_ERROR_FAILURE;
    proto->CacheOffsets(identity);

    // Set up the prototype on the global.
    MOZ_ASSERT(proto->GetJSProtoObject());
    success = JS_SplicePrototype(ccx, global, proto->GetJSProtoObject());
    if (!success)
        return NS_ERROR_FAILURE;

    // Construct the wrapper.
    nsRefPtr<XPCWrappedNative> wrapper = new XPCWrappedNative(identity, proto);

    // The wrapper takes over the strong reference to the native object.
    nativeHelper.forgetCanonical();

    //
    // We don't call ::Init() on this wrapper, because our setup requirements
    // are different for globals. We do our setup inline here, instead.
    //

    // Share mScriptableInfo with the proto.
    //
    // This is probably more trouble than it's worth, since we've already created
    // an XPCNativeScriptableInfo for ourselves. Moreover, most of that class is
    // shared internally via XPCNativeScriptableInfoShared, so the memory
    // savings are negligible. Nevertheless, this is what ::Init() does, and we
    // want to be as consistent as possible with that code.
    XPCNativeScriptableInfo* siProto = proto->GetScriptableInfo();
    if (siProto && siProto->GetCallback() == sciWrapper.GetCallback()) {
        wrapper->mScriptableInfo = siProto;
        delete si;
    } else {
        wrapper->mScriptableInfo = si;
    }

    // Set the JS object to the global we already created.
    wrapper->mFlatJSObject = global;

    // Set the private to the XPCWrappedNative.
    JS_SetPrivate(global, wrapper);

    // There are dire comments elsewhere in the code about how a GC can
    // happen somewhere after wrapper initialization but before the wrapper is
    // added to the hashtable in FinishCreate(). It's not clear if that can
    // happen here, but let's just be safe for now.
    AutoMarkingWrappedNativePtr wrapperMarker(ccx, wrapper);

    // Call the common Init finish routine. This mainly just does an AddRef
    // on behalf of XPConnect (the corresponding Release is in the finalizer
    // hook), but it does some other miscellaneous things too, so we don't
    // inline it.
    success = wrapper->FinishInit(ccx);
    MOZ_ASSERT(success);

    // Go through some extra work to find the tearoff. This is kind of silly
    // on a conceptual level: the point of tearoffs is to cache the results
    // of QI-ing mIdentity to different interfaces, and we don't need that
    // since we're dealing with nsISupports. But lots of code expects tearoffs
    // to exist for everything, so we just follow along.
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(ccx, &NS_GET_IID(nsISupports));
    MOZ_ASSERT(iface);
    nsresult status;
    success = wrapper->FindTearOff(ccx, iface, false, &status);
    if (!success)
        return status;

    // Call the common creation finish routine. This does all of the bookkeeping
    // like inserting the wrapper into the wrapper map and setting up the wrapper
    // cache.
    return FinishCreate(ccx, scope, iface, nativeHelper.GetWrapperCache(),
                        wrapper, wrappedGlobal);
}

// static
nsresult
XPCWrappedNative::GetNewOrUsed(XPCCallContext& ccx,
                               xpcObjectHelper& helper,
                               XPCWrappedNativeScope* Scope,
                               XPCNativeInterface* Interface,
                               XPCWrappedNative** resultWrapper)
{
    nsWrapperCache *cache = helper.GetWrapperCache();

    NS_ASSERTION(!cache || !cache->GetWrapperPreserveColor(),
                 "We assume the caller already checked if it could get the "
                 "wrapper from the cache.");

    nsresult rv;

    NS_ASSERTION(!Scope->GetRuntime()->GetThreadRunningGC(),
                 "XPCWrappedNative::GetNewOrUsed called during GC");

    nsISupports *identity = helper.GetCanonical();

    if (!identity) {
        NS_ERROR("This XPCOM object fails in QueryInterface to nsISupports!");
        return NS_ERROR_FAILURE;
    }

    XPCLock* mapLock = Scope->GetRuntime()->GetMapLock();

    nsRefPtr<XPCWrappedNative> wrapper;

    Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();
    if (!cache) {
        {   // scoped lock
            XPCAutoLock lock(mapLock);
            wrapper = map->Find(identity);
        }

        if (wrapper) {
            if (Interface &&
                !wrapper->FindTearOff(ccx, Interface, false, &rv)) {
                NS_ASSERTION(NS_FAILED(rv), "returning NS_OK on failure");
                return rv;
            }
            DEBUG_CheckWrapperThreadSafety(wrapper);
            *resultWrapper = wrapper.forget().get();
            return NS_OK;
        }
    }
#ifdef DEBUG
    else if (!cache->GetWrapperPreserveColor())
    {   // scoped lock
        XPCAutoLock lock(mapLock);
        NS_ASSERTION(!map->Find(identity),
                     "There's a wrapper in the hashtable but it wasn't cached?");
    }
#endif

    // There is a chance that the object wants to have the self-same JSObject
    // reflection regardless of the scope into which we are reflecting it.
    // Many DOM objects require this. The scriptable helper specifies this
    // in preCreate by indicating a 'parent' of a particular scope.
    //
    // To handle this we need to get the scriptable helper early and ask it.
    // It is possible that we will then end up forwarding this entire call
    // to this same function but with a different scope.

    // If we are making a wrapper for the nsIClassInfo interface then
    // We *don't* want to have it use the prototype meant for instances
    // of that class.
    JSBool isClassInfo = Interface &&
                         Interface->GetIID()->Equals(NS_GET_IID(nsIClassInfo));

    nsIClassInfo *info = helper.GetClassInfo();

    XPCNativeScriptableCreateInfo sciProto;
    XPCNativeScriptableCreateInfo sci;

    // Gather scriptable create info if we are wrapping something
    // other than an nsIClassInfo object. We need to not do this for
    // nsIClassInfo objects because often nsIClassInfo implementations
    // are also nsIXPCScriptable helper implementations, but the helper
    // code is obviously intended for the implementation of the class
    // described by the nsIClassInfo, not for the class info object
    // itself.
    const XPCNativeScriptableCreateInfo& sciWrapper =
        isClassInfo ? sci :
        GatherScriptableCreateInfo(identity, info, sciProto, sci);

    JSObject* parent = Scope->GetGlobalJSObject();

    jsval newParentVal = JSVAL_NULL;
    XPCMarkableJSVal newParentVal_markable(&newParentVal);
    AutoMarkingJSVal newParentVal_automarker(ccx, &newParentVal_markable);
    JSBool needsSOW = false;
    JSBool needsCOW = false;

    JSAutoEnterCompartment ac;

    if (sciWrapper.GetFlags().WantPreCreate()) {
        JSObject* plannedParent = parent;
        nsresult rv = sciWrapper.GetCallback()->PreCreate(identity, ccx,
                                                          parent, &parent);
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_SUCCESS_CHROME_ACCESS_ONLY)
            needsSOW = true;
        rv = NS_OK;

        NS_ASSERTION(!xpc::WrapperFactory::IsXrayWrapper(parent),
                     "Xray wrapper being used to parent XPCWrappedNative?");

        if (!ac.enter(ccx, parent))
            return NS_ERROR_FAILURE;

        if (parent != plannedParent) {
            XPCWrappedNativeScope* betterScope =
                XPCWrappedNativeScope::FindInJSObjectScope(ccx, parent);
            if (betterScope != Scope)
                return GetNewOrUsed(ccx, helper, betterScope, Interface, resultWrapper);

            newParentVal = OBJECT_TO_JSVAL(parent);
        }

        // Take the performance hit of checking the hashtable again in case
        // the preCreate call caused the wrapper to get created through some
        // interesting path (the DOM code tends to make this happen sometimes).

        if (cache) {
            JSObject *cached = cache->GetWrapper();
            if (cached) {
                if (IS_SLIM_WRAPPER_OBJECT(cached)) {
                    if (!XPCWrappedNative::Morph(ccx, cached, Interface, cache,
                                                 getter_AddRefs(wrapper)))
                        return NS_ERROR_FAILURE;
                } else {
                    wrapper = static_cast<XPCWrappedNative*>(xpc_GetJSPrivate(cached));
                }
            }
        } else {
            // scoped lock
            XPCAutoLock lock(mapLock);
            wrapper = map->Find(identity);
        }

        if (wrapper) {
            if (Interface && !wrapper->FindTearOff(ccx, Interface, false, &rv)) {
                NS_ASSERTION(NS_FAILED(rv), "returning NS_OK on failure");
                return rv;
            }
            DEBUG_CheckWrapperThreadSafety(wrapper);
            *resultWrapper = wrapper.forget().get();
            return NS_OK;
        }
    } else {
        if (!ac.enter(ccx, parent))
            return NS_ERROR_FAILURE;

        nsISupports *Object = helper.Object();
        if (nsXPCWrappedJSClass::IsWrappedJS(Object)) {
            nsCOMPtr<nsIXPConnectWrappedJS> wrappedjs(do_QueryInterface(Object));
            JSObject *obj;
            wrappedjs->GetJSObject(&obj);
            if (xpc::AccessCheck::isChrome(js::GetObjectCompartment(obj)) &&
                !xpc::AccessCheck::isChrome(js::GetObjectCompartment(Scope->GetGlobalJSObject()))) {
                needsCOW = true;
            }
        }
    }

    AutoMarkingWrappedNativeProtoPtr proto(ccx);

    // If there is ClassInfo (and we are not building a wrapper for the
    // nsIClassInfo interface) then we use a wrapper that needs a prototype.

    // Note that the security check happens inside FindTearOff - after the
    // wrapper is actually created, but before JS code can see it.

    if (info && !isClassInfo) {
        proto = XPCWrappedNativeProto::GetNewOrUsed(ccx, Scope, info, &sciProto);
        if (!proto)
            return NS_ERROR_FAILURE;

        proto->CacheOffsets(identity);

        wrapper = new XPCWrappedNative(identity, proto);
        if (!wrapper)
            return NS_ERROR_FAILURE;
    } else {
        AutoMarkingNativeInterfacePtr iface(ccx, Interface);
        if (!iface)
            iface = XPCNativeInterface::GetISupports(ccx);

        AutoMarkingNativeSetPtr set(ccx);
        set = XPCNativeSet::GetNewOrUsed(ccx, nsnull, iface, 0);

        if (!set)
            return NS_ERROR_FAILURE;

        wrapper = new XPCWrappedNative(identity, Scope, set);
        if (!wrapper)
            return NS_ERROR_FAILURE;

        DEBUG_ReportShadowedMembers(set, wrapper, nsnull);
    }

    // The strong reference was taken over by the wrapper, so make the nsCOMPtr
    // forget about it.
    helper.forgetCanonical();

    NS_ASSERTION(!xpc::WrapperFactory::IsXrayWrapper(parent),
                 "Xray wrapper being used to parent XPCWrappedNative?");

    // We use an AutoMarkingPtr here because it is possible for JS gc to happen
    // after we have Init'd the wrapper but *before* we add it to the hashtable.
    // This would cause the mSet to get collected and we'd later crash. I've
    // *seen* this happen.
    AutoMarkingWrappedNativePtr wrapperMarker(ccx, wrapper);

    if (!wrapper->Init(ccx, parent, &sciWrapper))
        return NS_ERROR_FAILURE;

    if (Interface && !wrapper->FindTearOff(ccx, Interface, false, &rv)) {
        NS_ASSERTION(NS_FAILED(rv), "returning NS_OK on failure");
        return rv;
    }

    if (needsSOW)
        wrapper->SetNeedsSOW();
    if (needsCOW)
        wrapper->SetNeedsCOW();

    return FinishCreate(ccx, Scope, Interface, cache, wrapper, resultWrapper);
}

static nsresult
FinishCreate(XPCCallContext& ccx,
             XPCWrappedNativeScope* Scope,
             XPCNativeInterface* Interface,
             nsWrapperCache *cache,
             XPCWrappedNative* inWrapper,
             XPCWrappedNative** resultWrapper)
{
    MOZ_ASSERT(inWrapper);

#if DEBUG_xpc_leaks
    {
        char* s = wrapper->ToString(ccx);
        NS_ASSERTION(wrapper->IsValid(), "eh?");
        printf("Created wrapped native %s, flat JSObject is %p\n",
               s, (void*)wrapper->GetFlatJSObjectNoMark());
        if (s)
            JS_smprintf_free(s);
    }
#endif

    XPCLock* mapLock = Scope->GetRuntime()->GetMapLock();
    Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();

    nsRefPtr<XPCWrappedNative> wrapper;
    {   // scoped lock

        // Deal with the case where the wrapper got created as a side effect
        // of one of our calls out of this code (or on another thread). Add()
        // returns the (possibly pre-existing) wrapper that ultimately ends up
        // in the map, which is what we want.
        XPCAutoLock lock(mapLock);
        wrapper = map->Add(inWrapper);
        if (!wrapper)
            return NS_ERROR_FAILURE;
    }

    if (wrapper == inWrapper) {
        JSObject *flat = wrapper->GetFlatJSObject();
        NS_ASSERTION(!cache || !cache->GetWrapperPreserveColor() ||
                     flat == cache->GetWrapperPreserveColor(),
                     "This object has a cached wrapper that's different from "
                     "the JSObject held by its native wrapper?");

        if (cache && !cache->GetWrapperPreserveColor())
            cache->SetWrapper(flat);

        // Our newly created wrapper is the one that we just added to the table.
        // All is well. Call PostCreate as necessary.
        XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
        if (si && si->GetFlags().WantPostCreate()) {
            nsresult rv = si->GetCallback()->PostCreate(wrapper, ccx, flat);
            if (NS_FAILED(rv)) {
                // PostCreate failed and that's Very Bad. We'll remove it from
                // the map and mark it as invalid, but the PostCreate function
                // may have handed the partially-constructed-and-now-invalid
                // wrapper to someone before failing. Or, perhaps worse, the
                // PostCreate call could have triggered code that reentered
                // XPConnect and tried to wrap the same object. In that case
                // *we* hand out the invalid wrapper since it is already in our
                // map :(
                NS_ERROR("PostCreate failed! This is known to cause "
                         "inconsistent state for some class types and may even "
                         "cause a crash in combination with a JS GC. Fix the "
                         "failing PostCreate ASAP!");

                {   // scoped lock
                    XPCAutoLock lock(mapLock);
                    map->Remove(wrapper);
                }

                // This would be a good place to tell the wrapper not to remove
                // itself from the map when it dies... See bug 429442.

                if (cache)
                    cache->ClearWrapper();
                wrapper->Release();
                return rv;
            }
        }
    }

    DEBUG_CheckClassInfoClaims(wrapper);
    *resultWrapper = wrapper.forget().get();
    return NS_OK;
}

// static
nsresult
XPCWrappedNative::Morph(XPCCallContext& ccx,
                        JSObject* existingJSObject,
                        XPCNativeInterface* Interface,
                        nsWrapperCache *cache,
                        XPCWrappedNative** resultWrapper)
{
    NS_ASSERTION(IS_SLIM_WRAPPER(existingJSObject),
                 "Trying to morph a JSObject that's not a slim wrapper?");

    nsISupports *identity =
        static_cast<nsISupports*>(xpc_GetJSPrivate(existingJSObject));
    XPCWrappedNativeProto *proto = GetSlimWrapperProto(existingJSObject);

#if DEBUG
    // FIXME Can't assert this until
    //       https://bugzilla.mozilla.org/show_bug.cgi?id=343141 is fixed.
#if 0
    if (proto->GetScriptableInfo()->GetFlags().WantPreCreate()) {
        JSObject* parent = JS_GetParent(existingJSObject);
        JSObject* plannedParent = parent;
        nsresult rv =
            proto->GetScriptableInfo()->GetCallback()->PreCreate(identity, ccx,
                                                                 parent,
                                                                 &parent);
        if (NS_FAILED(rv))
            return rv;

        NS_ASSERTION(parent == plannedParent,
                     "PreCreate returned a different parent");
    }
#endif
#endif

    nsRefPtr<XPCWrappedNative> wrapper = new XPCWrappedNative(dont_AddRef(identity), proto);
    if (!wrapper)
        return NS_ERROR_FAILURE;

    NS_ASSERTION(!xpc::WrapperFactory::IsXrayWrapper(js::GetObjectParent(existingJSObject)),
                 "Xray wrapper being used to parent XPCWrappedNative?");

    // We use an AutoMarkingPtr here because it is possible for JS gc to happen
    // after we have Init'd the wrapper but *before* we add it to the hashtable.
    // This would cause the mSet to get collected and we'd later crash. I've
    // *seen* this happen.
    AutoMarkingWrappedNativePtr wrapperMarker(ccx, wrapper);

    JSAutoEnterCompartment ac;
    if (!ac.enter(ccx, existingJSObject)) {
        wrapper->mIdentity = nsnull;
        return NS_ERROR_FAILURE;
    }
    if (!wrapper->Init(ccx, existingJSObject))
        return NS_ERROR_FAILURE;

    nsresult rv;
    if (Interface && !wrapper->FindTearOff(ccx, Interface, false, &rv)) {
        NS_ASSERTION(NS_FAILED(rv), "returning NS_OK on failure");
        return rv;
    }

    return FinishCreate(ccx, wrapper->GetScope(), Interface, cache, wrapper, resultWrapper);
}

// static
nsresult
XPCWrappedNative::GetUsedOnly(XPCCallContext& ccx,
                              nsISupports* Object,
                              XPCWrappedNativeScope* Scope,
                              XPCNativeInterface* Interface,
                              XPCWrappedNative** resultWrapper)
{
    NS_ASSERTION(Object, "XPCWrappedNative::GetUsedOnly was called with a null Object");

    XPCWrappedNative* wrapper;
    nsWrapperCache* cache = nsnull;
    CallQueryInterface(Object, &cache);
    if (cache) {
        JSObject *flat = cache->GetWrapper();
        if (flat && IS_SLIM_WRAPPER_OBJECT(flat) && !MorphSlimWrapper(ccx, flat))
           return NS_ERROR_FAILURE;

        wrapper = flat ?
                  static_cast<XPCWrappedNative*>(xpc_GetJSPrivate(flat)) :
                  nsnull;

        if (!wrapper) {
            *resultWrapper = nsnull;
            return NS_OK;
        }
        NS_ADDREF(wrapper);
    } else {
        nsCOMPtr<nsISupports> identity = do_QueryInterface(Object);

        if (!identity) {
            NS_ERROR("This XPCOM object fails in QueryInterface to nsISupports!");
            return NS_ERROR_FAILURE;
        }

        Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();

        {   // scoped lock
            XPCAutoLock lock(Scope->GetRuntime()->GetMapLock());
            wrapper = map->Find(identity);
            if (!wrapper) {
                *resultWrapper = nsnull;
                return NS_OK;
            }
            NS_ADDREF(wrapper);
        }
    }

    nsresult rv;
    if (Interface && !wrapper->FindTearOff(ccx, Interface, false, &rv)) {
        NS_RELEASE(wrapper);
        NS_ASSERTION(NS_FAILED(rv), "returning NS_OK on failure");
        return rv;
    }

    *resultWrapper = wrapper;
    return NS_OK;
}

// This ctor is used if this object will have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports> aIdentity,
                                   XPCWrappedNativeProto* aProto)
    : mMaybeProto(aProto),
      mSet(aProto->GetSet()),
      mFlatJSObject(INVALID_OBJECT), // non-null to pass IsValid() test
      mScriptableInfo(nsnull),
      mWrapperWord(0)
#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
    , mThread(PR_GetCurrentThread())
#endif
{
    mIdentity = aIdentity.get();

    NS_ASSERTION(mMaybeProto, "bad ctor param");
    NS_ASSERTION(mSet, "bad ctor param");

    DEBUG_TrackNewWrapper(this);
}

// This ctor is used if this object will NOT have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports> aIdentity,
                                   XPCWrappedNativeScope* aScope,
                                   XPCNativeSet* aSet)

    : mMaybeScope(TagScope(aScope)),
      mSet(aSet),
      mFlatJSObject(INVALID_OBJECT), // non-null to pass IsValid() test
      mScriptableInfo(nsnull),
      mWrapperWord(0)
#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
    , mThread(PR_GetCurrentThread())
#endif
{
    mIdentity = aIdentity.get();

    NS_ASSERTION(aScope, "bad ctor param");
    NS_ASSERTION(aSet, "bad ctor param");

    DEBUG_TrackNewWrapper(this);
}

XPCWrappedNative::~XPCWrappedNative()
{
    DEBUG_TrackDeleteWrapper(this);

    Destroy();
}

static const intptr_t WRAPPER_WORD_POISON = 0xa8a8a8a8;

void
XPCWrappedNative::Destroy()
{
    XPCWrappedNativeProto* proto = GetProto();

    if (mScriptableInfo &&
        (!HasProto() ||
         (proto && proto->GetScriptableInfo() != mScriptableInfo))) {
        delete mScriptableInfo;
        mScriptableInfo = nsnull;
    }

    XPCWrappedNativeScope *scope = GetScope();
    if (scope) {
        Native2WrappedNativeMap* map = scope->GetWrappedNativeMap();

        // scoped lock
        XPCAutoLock lock(GetRuntime()->GetMapLock());

        // Post-1.9 we should not remove this wrapper from the map if it is
        // uninitialized.
        map->Remove(this);
    }

    if (mIdentity) {
        XPCJSRuntime* rt = GetRuntime();
        if (rt && rt->GetDoingFinalization()) {
            if (rt->DeferredRelease(mIdentity)) {
                mIdentity = nsnull;
            } else {
                NS_WARNING("Failed to append object for deferred release.");
                // XXX do we really want to do this???
                NS_RELEASE(mIdentity);
            }
        } else {
            NS_RELEASE(mIdentity);
        }
    }

    /*
     * The only time GetRuntime() will be NULL is if Destroy is called a second
     * time on a wrapped native. Since we already unregistered the pointer the
     * first time, there's no need to unregister again. Unregistration is safe
     * the first time because mWrapperWord isn't used afterwards.
     */
    if (XPCJSRuntime *rt = GetRuntime()) {
        if (js::IsIncrementalBarrierNeeded(rt->GetJSRuntime()))
            js::IncrementalReferenceBarrier(GetWrapperPreserveColor());
        mWrapperWord = WRAPPER_WORD_POISON;
    } else {
        MOZ_ASSERT(mWrapperWord == WRAPPER_WORD_POISON);
    }

    mMaybeScope = nsnull;
}

void
XPCWrappedNative::UpdateScriptableInfo(XPCNativeScriptableInfo *si)
{
    NS_ASSERTION(mScriptableInfo, "UpdateScriptableInfo expects an existing scriptable info");

    // Write barrier for incremental GC.
    JSRuntime* rt = GetRuntime()->GetJSRuntime();
    if (js::IsIncrementalBarrierNeeded(rt))
        mScriptableInfo->Mark();

    mScriptableInfo = si;
}

void
XPCWrappedNative::SetProto(XPCWrappedNativeProto* p)
{
    NS_ASSERTION(!IsWrapperExpired(), "bad ptr!");

    MOZ_ASSERT(HasProto());

    // Write barrier for incremental GC.
    JSRuntime* rt = GetRuntime()->GetJSRuntime();
    GetProto()->WriteBarrierPre(rt);

    mMaybeProto = p;
}

// This is factored out so that it can be called publicly
// static
void
XPCWrappedNative::GatherProtoScriptableCreateInfo(nsIClassInfo* classInfo,
                                                  XPCNativeScriptableCreateInfo& sciProto)
{
    NS_ASSERTION(classInfo, "bad param");
    NS_ASSERTION(!sciProto.GetCallback(), "bad param");

    nsXPCClassInfo *classInfoHelper = nsnull;
    CallQueryInterface(classInfo, &classInfoHelper);
    if (classInfoHelper) {
        nsCOMPtr<nsIXPCScriptable> helper =
          dont_AddRef(static_cast<nsIXPCScriptable*>(classInfoHelper));
        uint32_t flags;
        nsresult rv = classInfoHelper->GetScriptableFlags(&flags);
        if (NS_FAILED(rv))
            flags = 0;

        sciProto.SetCallback(helper.forget());
        sciProto.SetFlags(flags);
        sciProto.SetInterfacesBitmap(classInfoHelper->GetInterfacesBitmap());

        return;
    }

    nsCOMPtr<nsISupports> possibleHelper;
    nsresult rv = classInfo->GetHelperForLanguage(nsIProgrammingLanguage::JAVASCRIPT,
                                                  getter_AddRefs(possibleHelper));
    if (NS_SUCCEEDED(rv) && possibleHelper) {
        nsCOMPtr<nsIXPCScriptable> helper(do_QueryInterface(possibleHelper));
        if (helper) {
            uint32_t flags;
            rv = helper->GetScriptableFlags(&flags);
            if (NS_FAILED(rv))
                flags = 0;

            sciProto.SetCallback(helper.forget());
            sciProto.SetFlags(flags);
        }
    }
}

// static
const XPCNativeScriptableCreateInfo&
XPCWrappedNative::GatherScriptableCreateInfo(nsISupports* obj,
                                             nsIClassInfo* classInfo,
                                             XPCNativeScriptableCreateInfo& sciProto,
                                             XPCNativeScriptableCreateInfo& sciWrapper)
{
    NS_ASSERTION(!sciWrapper.GetCallback(), "bad param");

    // Get the class scriptable helper (if present)
    if (classInfo) {
        GatherProtoScriptableCreateInfo(classInfo, sciProto);

        if (sciProto.GetFlags().DontAskInstanceForScriptable())
            return sciProto;
    }

    // Do the same for the wrapper specific scriptable
    nsCOMPtr<nsIXPCScriptable> helper(do_QueryInterface(obj));
    if (helper) {
        uint32_t flags;
        nsresult rv = helper->GetScriptableFlags(&flags);
        if (NS_FAILED(rv))
            flags = 0;

        sciWrapper.SetCallback(helper.forget());
        sciWrapper.SetFlags(flags);

        // A whole series of assertions to catch bad uses of scriptable flags on
        // the siWrapper...

        NS_ASSERTION(!(sciWrapper.GetFlags().WantPreCreate() &&
                       !sciProto.GetFlags().WantPreCreate()),
                     "Can't set WANT_PRECREATE on an instance scriptable "
                     "without also setting it on the class scriptable");

        NS_ASSERTION(!(sciWrapper.GetFlags().DontEnumStaticProps() &&
                       !sciProto.GetFlags().DontEnumStaticProps() &&
                       sciProto.GetCallback()),
                     "Can't set DONT_ENUM_STATIC_PROPS on an instance scriptable "
                     "without also setting it on the class scriptable (if present and shared)");

        NS_ASSERTION(!(sciWrapper.GetFlags().DontEnumQueryInterface() &&
                       !sciProto.GetFlags().DontEnumQueryInterface() &&
                       sciProto.GetCallback()),
                     "Can't set DONT_ENUM_QUERY_INTERFACE on an instance scriptable "
                     "without also setting it on the class scriptable (if present and shared)");

        NS_ASSERTION(!(sciWrapper.GetFlags().DontAskInstanceForScriptable() &&
                       !sciProto.GetFlags().DontAskInstanceForScriptable()),
                     "Can't set DONT_ASK_INSTANCE_FOR_SCRIPTABLE on an instance scriptable "
                     "without also setting it on the class scriptable");

        NS_ASSERTION(!(sciWrapper.GetFlags().ClassInfoInterfacesOnly() &&
                       !sciProto.GetFlags().ClassInfoInterfacesOnly() &&
                       sciProto.GetCallback()),
                     "Can't set CLASSINFO_INTERFACES_ONLY on an instance scriptable "
                     "without also setting it on the class scriptable (if present and shared)");

        NS_ASSERTION(!(sciWrapper.GetFlags().AllowPropModsDuringResolve() &&
                       !sciProto.GetFlags().AllowPropModsDuringResolve() &&
                       sciProto.GetCallback()),
                     "Can't set ALLOW_PROP_MODS_DURING_RESOLVE on an instance scriptable "
                     "without also setting it on the class scriptable (if present and shared)");

        NS_ASSERTION(!(sciWrapper.GetFlags().AllowPropModsToPrototype() &&
                       !sciProto.GetFlags().AllowPropModsToPrototype() &&
                       sciProto.GetCallback()),
                     "Can't set ALLOW_PROP_MODS_TO_PROTOTYPE on an instance scriptable "
                     "without also setting it on the class scriptable (if present and shared)");

        return sciWrapper;
    }

    return sciProto;
}

#ifdef DEBUG_slimwrappers
static PRUint32 sMorphedSlimWrappers;
#endif

JSBool
XPCWrappedNative::Init(XPCCallContext& ccx, JSObject* parent,
                       const XPCNativeScriptableCreateInfo* sci)
{
    // setup our scriptable info...

    if (sci->GetCallback()) {
        if (HasProto()) {
            XPCNativeScriptableInfo* siProto = GetProto()->GetScriptableInfo();
            if (siProto && siProto->GetCallback() == sci->GetCallback())
                mScriptableInfo = siProto;
        }
        if (!mScriptableInfo) {
            mScriptableInfo =
                XPCNativeScriptableInfo::Construct(ccx, sci);

            if (!mScriptableInfo)
                return false;
        }
    }
    XPCNativeScriptableInfo* si = mScriptableInfo;

    // create our flatJSObject

    JSClass* jsclazz = si ? si->GetJSClass() : Jsvalify(&XPC_WN_NoHelper_JSClass.base);

    // We should have the global jsclass flag if and only if we're a global.
    MOZ_ASSERT_IF(si, !!si->GetFlags().IsGlobalObject() == !!(jsclazz->flags & JSCLASS_IS_GLOBAL));

    NS_ASSERTION(jsclazz &&
                 jsclazz->name &&
                 jsclazz->flags &&
                 jsclazz->addProperty &&
                 jsclazz->delProperty &&
                 jsclazz->getProperty &&
                 jsclazz->setProperty &&
                 jsclazz->enumerate &&
                 jsclazz->resolve &&
                 jsclazz->convert &&
                 jsclazz->finalize, "bad class");

    JSObject* protoJSObject = HasProto() ?
                                GetProto()->GetJSProtoObject() :
                                GetScope()->GetPrototypeNoHelper(ccx);

    if (!protoJSObject) {
        return false;
    }

    mFlatJSObject = xpc_NewSystemInheritingJSObject(ccx, jsclazz, protoJSObject, false, parent);
    if (!mFlatJSObject)
        return false;

    JS_SetPrivate(mFlatJSObject, this);

    return FinishInit(ccx);
}

JSBool
XPCWrappedNative::Init(XPCCallContext &ccx, JSObject *existingJSObject)
{
    JS_SetPrivate(existingJSObject, this);

    // Morph the existing object.
    JS_SetReservedSlot(existingJSObject, 0, JSVAL_VOID);

    mScriptableInfo = GetProto()->GetScriptableInfo();
    mFlatJSObject = existingJSObject;

    SLIM_LOG(("----- %i morphed slim wrapper (mFlatJSObject: %p, %p)\n",
              ++sMorphedSlimWrappers, mFlatJSObject,
              static_cast<nsISupports*>(xpc_GetJSPrivate(mFlatJSObject))));

    return FinishInit(ccx);
}

JSBool
XPCWrappedNative::FinishInit(XPCCallContext &ccx)
{
    // This reference will be released when mFlatJSObject is finalized.
    // Since this reference will push the refcount to 2 it will also root
    // mFlatJSObject;
    NS_ASSERTION(1 == mRefCnt, "unexpected refcount value");
    NS_ADDREF(this);

    if (mScriptableInfo && mScriptableInfo->GetFlags().WantCreate() &&
        NS_FAILED(mScriptableInfo->GetCallback()->Create(this, ccx,
                                                         mFlatJSObject))) {
        return false;
    }

#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
    NS_ASSERTION(mThread, "Should have been set at construction time!");

    if (HasProto() && GetProto()->ClassIsMainThreadOnly() && !NS_IsMainThread()) {
        DEBUG_ReportWrapperThreadSafetyError(ccx,
                                             "MainThread only wrapper created on the wrong thread", this);
        return false;
    }
#endif

    // A hack for bug 517665, increase the probability for GC.
    JS_updateMallocCounter(ccx.GetJSContext(), 2 * sizeof(XPCWrappedNative));

    return true;
}


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCWrappedNative)
  NS_INTERFACE_MAP_ENTRY(nsIXPConnectWrappedNative)
  NS_INTERFACE_MAP_ENTRY(nsIXPConnectJSObjectHolder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPConnectWrappedNative)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(XPCWrappedNative)
NS_IMPL_THREADSAFE_RELEASE(XPCWrappedNative)

/*
 *  Wrapped Native lifetime management is messy!
 *
 *  - At creation we push the refcount to 2 (only one of which is owned by
 *    the native caller that caused the wrapper creation).
 *  - During the JS GC Mark phase we mark any wrapper with a refcount > 1.
 *  - The *only* thing that can make the wrapper get destroyed is the
 *    finalization of mFlatJSObject. And *that* should only happen if the only
 *    reference is the single extra (internal) reference we hold.
 *
 *  - The wrapper has a pointer to the nsISupports 'view' of the wrapped native
 *    object i.e... mIdentity. This is held until the wrapper's refcount goes
 *    to zero and the wrapper is released, or until an expired wrapper (i.e.,
 *    one unlinked by the cycle collector) has had its JS object finalized.
 *
 *  - The wrapper also has 'tearoffs'. It has one tearoff for each interface
 *    that is actually used on the native object. 'Used' means we have either
 *    needed to QueryInterface to verify the availability of that interface
 *    of that we've had to QueryInterface in order to actually make a call
 *    into the wrapped object via the pointer for the given interface.
 *
 *  - Each tearoff's 'mNative' member (if non-null) indicates one reference
 *    held by our wrapper on the wrapped native for the given interface
 *    associated with the tearoff. If we release that reference then we set
 *    the tearoff's 'mNative' to null.
 *
 *  - We use the occasion of the JavaScript GCCallback for the JSGC_MARK_END
 *    event to scan the tearoffs of all wrappers for non-null mNative members
 *    that represent unused references. We can tell that a given tearoff's
 *    mNative is unused by noting that no live XPCCallContexts hold a pointer
 *    to the tearoff.
 *
 *  - As a time/space tradeoff we may decide to not do this scanning on
 *    *every* JavaScript GC. We *do* want to do this *sometimes* because
 *    we want to allow for wrapped native's to do their own tearoff patterns.
 *    So, we want to avoid holding references to interfaces that we don't need.
 *    At the same time, we don't want to be bracketing every call into a
 *    wrapped native object with a QueryInterface/Release pair. And we *never*
 *    make a call into the object except via the correct interface for which
 *    we've QI'd.
 *
 *  - Each tearoff *can* have a mJSObject whose lazily resolved properties
 *    represent the methods/attributes/constants of that specific interface.
 *    This is optionally reflected into JavaScript as "foo.nsIFoo" when "foo"
 *    is the name of mFlatJSObject and "nsIFoo" is the name of the given
 *    interface associated with the tearoff. When we create the tearoff's
 *    mJSObject we set it's parent to be mFlatJSObject. This way we know that
 *    when mFlatJSObject get's collected there are no outstanding reachable
 *    tearoff mJSObjects. Note that we must clear the private of any lingering
 *    mJSObjects at this point because we have no guarentee of the *order* of
 *    finalization within a given gc cycle.
 */

void
XPCWrappedNative::FlatJSObjectFinalized()
{
    if (!IsValid())
        return;

    // Iterate the tearoffs and null out each of their JSObject's privates.
    // This will keep them from trying to access their pointers to the
    // dying tearoff object. We can safely assume that those remaining
    // JSObjects are about to be finalized too.

    XPCWrappedNativeTearOffChunk* chunk;
    for (chunk = &mFirstChunk; chunk; chunk = chunk->mNextChunk) {
        XPCWrappedNativeTearOff* to = chunk->mTearOffs;
        for (int i = XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK-1; i >= 0; i--, to++) {
            JSObject* jso = to->GetJSObjectPreserveColor();
            if (jso) {
                NS_ASSERTION(JS_IsAboutToBeFinalized(jso), "bad!");
                JS_SetPrivate(jso, nsnull);
                to->JSObjectFinalized();
            }

            // We also need to release any native pointers held...
            nsISupports* obj = to->GetNative();
            if (obj) {
#ifdef XP_WIN
                // Try to detect free'd pointer
                NS_ASSERTION(*(int*)obj != 0xdddddddd, "bad pointer!");
                NS_ASSERTION(*(int*)obj != 0,          "bad pointer!");
#endif
                XPCJSRuntime* rt = GetRuntime();
                if (rt) {
                    if (!rt->DeferredRelease(obj)) {
                        NS_WARNING("Failed to append object for deferred release.");
                        // XXX do we really want to do this???
                        obj->Release();
                    }
                } else {
                    obj->Release();
                }
                to->SetNative(nsnull);
            }

            to->SetInterface(nsnull);
        }
    }

    nsWrapperCache *cache = nsnull;
    CallQueryInterface(mIdentity, &cache);
    if (cache)
        cache->ClearWrapper();

    // This makes IsValid return false from now on...
    mFlatJSObject = nsnull;

    NS_ASSERTION(mIdentity, "bad pointer!");
#ifdef XP_WIN
    // Try to detect free'd pointer
    NS_ASSERTION(*(int*)mIdentity != 0xdddddddd, "bad pointer!");
    NS_ASSERTION(*(int*)mIdentity != 0,          "bad pointer!");
#endif

    if (IsWrapperExpired()) {
        Destroy();
    }

    // Note that it's not safe to touch mNativeWrapper here since it's
    // likely that it has already been finalized.

    Release();
}

void
XPCWrappedNative::SystemIsBeingShutDown()
{
#ifdef DEBUG_xpc_hacker
    {
        printf("Removing root for still-live XPCWrappedNative %p wrapping:\n",
               static_cast<void*>(this));
        for (PRUint16 i = 0, i_end = mSet->GetInterfaceCount(); i < i_end; ++i) {
            nsXPIDLCString name;
            mSet->GetInterfaceAt(i)->GetInterfaceInfo()
                ->GetName(getter_Copies(name));
            printf("  %s\n", name.get());
        }
    }
#endif
    DEBUG_TrackShutdownWrapper(this);

    if (!IsValid())
        return;

    // The long standing strategy is to leak some objects still held at shutdown.
    // The general problem is that propagating release out of xpconnect at
    // shutdown time causes a world of problems.

    // We leak mIdentity (see above).

    // short circuit future finalization
    JS_SetPrivate(mFlatJSObject, nsnull);
    mFlatJSObject = nsnull; // This makes 'IsValid()' return false.

    XPCWrappedNativeProto* proto = GetProto();

    if (HasProto())
        proto->SystemIsBeingShutDown();

    if (mScriptableInfo &&
        (!HasProto() ||
         (proto && proto->GetScriptableInfo() != mScriptableInfo))) {
        delete mScriptableInfo;
    }

    // cleanup the tearoffs...

    XPCWrappedNativeTearOffChunk* chunk;
    for (chunk = &mFirstChunk; chunk; chunk = chunk->mNextChunk) {
        XPCWrappedNativeTearOff* to = chunk->mTearOffs;
        for (int i = XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK-1; i >= 0; i--, to++) {
            if (JSObject *jso = to->GetJSObjectPreserveColor()) {
                JS_SetPrivate(jso, nsnull);
                to->SetJSObject(nsnull);
            }
            // We leak the tearoff mNative
            // (for the same reason we leak mIdentity - see above).
            to->SetNative(nsnull);
            to->SetInterface(nsnull);
        }
    }

    if (mFirstChunk.mNextChunk) {
        delete mFirstChunk.mNextChunk;
        mFirstChunk.mNextChunk = nsnull;
    }
}

/***************************************************************************/

// If we have to transplant an object across compartments, we need to be
// careful if the underlying object implements nsWrapperCache and is preserving
// the wrapper.
//
// The class brackets a pair of Unpreserve/Preserve calls in the given scope.
//
// This class _must_ live on the stack, in part so that mPreservedWrapper is
// visible to the stack scanner. The caller wants the wrapper to be preserved,
// so we don't want it to get accidentally GCed.
class AutoWrapperChanger NS_STACK_CLASS {
public:
    AutoWrapperChanger() : mCache(nsnull)
                         , mCOMObj(nsnull)
                         , mPreservedWrapper(nsnull)
    {}

    void init(nsISupports* aCOMObj, nsWrapperCache* aWrapperCache) {
        mCOMObj = aCOMObj;
        mCache = aWrapperCache;
        if (mCache->PreservingWrapper()) {
            mPreservedWrapper = mCache->GetWrapper();
            MOZ_ASSERT(mPreservedWrapper);
            nsContentUtils::ReleaseWrapper(mCOMObj, mCache);
        }
    }

    ~AutoWrapperChanger() {
        if (mPreservedWrapper)
            nsContentUtils::PreserveWrapper(mCOMObj, mCache);
    }

private:
    nsWrapperCache* mCache;
    nsISupports* mCOMObj;
    JSObject* mPreservedWrapper;
};

// static
nsresult
XPCWrappedNative::ReparentWrapperIfFound(XPCCallContext& ccx,
                                         XPCWrappedNativeScope* aOldScope,
                                         XPCWrappedNativeScope* aNewScope,
                                         JSObject* aNewParent,
                                         nsISupports* aCOMObj,
                                         XPCWrappedNative** aWrapper)
{
    XPCNativeInterface* iface =
        XPCNativeInterface::GetISupports(ccx);

    if (!iface)
        return NS_ERROR_FAILURE;

    nsresult rv;

    nsRefPtr<XPCWrappedNative> wrapper;
    AutoWrapperChanger wrapperChanger;
    JSObject *flat;
    nsWrapperCache* cache = nsnull;
    CallQueryInterface(aCOMObj, &cache);
    if (cache) {

        // There's a wrapper cache. Make sure we keep it sane no matter what
        // happens.
        wrapperChanger.init(aCOMObj, cache);

        flat = cache->GetWrapper();
        if (flat && !IS_SLIM_WRAPPER_OBJECT(flat)) {
            wrapper = static_cast<XPCWrappedNative*>(xpc_GetJSPrivate(flat));
            NS_ASSERTION(wrapper->GetScope() == aOldScope,
                         "Incorrect scope passed");
        }
    } else {
        rv = XPCWrappedNative::GetUsedOnly(ccx, aCOMObj, aOldScope, iface,
                                           getter_AddRefs(wrapper));
        if (NS_FAILED(rv))
            return rv;

        flat = wrapper->GetFlatJSObject();
    }

    if (!flat) {
        *aWrapper = nsnull;
        return NS_OK;
    }

    // ReparentWrapperIfFound is really only meant to be called from DOM code
    // which must happen only on the main thread. Bail if we're on some other
    // thread or have a non-main-thread-only wrapper.
    if (!XPCPerThreadData::IsMainThread(ccx) ||
        (wrapper &&
         wrapper->GetProto() &&
         !wrapper->GetProto()->ClassIsMainThreadOnly())) {
        return NS_ERROR_FAILURE;
    }

    JSAutoEnterCompartment ac;
    if (!ac.enter(ccx, aNewScope->GetGlobalJSObject()))
        return NS_ERROR_FAILURE;

    if (aOldScope != aNewScope) {
        // Oh, so now we need to move the wrapper to a different scope.
        AutoMarkingWrappedNativeProtoPtr oldProto(ccx);
        AutoMarkingWrappedNativeProtoPtr newProto(ccx);

        // Cross-scope means cross-compartment.
        MOZ_ASSERT(js::GetObjectCompartment(aOldScope->GetGlobalJSObject()) !=
                   js::GetObjectCompartment(aNewScope->GetGlobalJSObject()));
        NS_ASSERTION(aNewParent, "won't be able to find the new parent");
        NS_ASSERTION(wrapper, "can't transplant slim wrappers");

        if (!wrapper)
            oldProto = GetSlimWrapperProto(flat);
        else if (wrapper->HasProto())
            oldProto = wrapper->GetProto();

        if (oldProto) {
            XPCNativeScriptableInfo *info = oldProto->GetScriptableInfo();
            XPCNativeScriptableCreateInfo ci(*info);
            newProto =
                XPCWrappedNativeProto::GetNewOrUsed(ccx, aNewScope,
                                                    oldProto->GetClassInfo(),
                                                    &ci, oldProto->GetOffsetsMasked());
            if (!newProto) {
                return NS_ERROR_FAILURE;
            }
        }

        if (wrapper) {
            Native2WrappedNativeMap* oldMap = aOldScope->GetWrappedNativeMap();
            Native2WrappedNativeMap* newMap = aNewScope->GetWrappedNativeMap();

            {   // scoped lock
                XPCAutoLock lock(aOldScope->GetRuntime()->GetMapLock());

                oldMap->Remove(wrapper);

                if (wrapper->HasProto())
                    wrapper->SetProto(newProto);

                // If the wrapper has no scriptable or it has a non-shared
                // scriptable, then we don't need to mess with it.
                // Otherwise...

                if (wrapper->mScriptableInfo &&
                    wrapper->mScriptableInfo == oldProto->GetScriptableInfo()) {
                    // The new proto had better have the same JSClass stuff as
                    // the old one! We maintain a runtime wide unique map of
                    // this stuff. So, if these don't match then the caller is
                    // doing something bad here.

                    NS_ASSERTION(oldProto->GetScriptableInfo()->GetScriptableShared() ==
                                 newProto->GetScriptableInfo()->GetScriptableShared(),
                                 "Changing proto is also changing JSObject Classname or "
                                 "helper's nsIXPScriptable flags. This is not allowed!");

                    wrapper->UpdateScriptableInfo(newProto->GetScriptableInfo());
                }

                NS_ASSERTION(!newMap->Find(wrapper->GetIdentityObject()),
                             "wrapper already in new scope!");

                (void) newMap->Add(wrapper);
            }

            JSObject *newobj = JS_CloneObject(ccx, flat,
                                              newProto->GetJSProtoObject(),
                                              aNewParent);
            if (!newobj)
                return NS_ERROR_FAILURE;

            JSObject *propertyHolder =
                JS_NewObjectWithGivenProto(ccx, NULL, NULL, aNewParent);
            if (!propertyHolder || !JS_CopyPropertiesFrom(ccx, propertyHolder, flat))
                return NS_ERROR_OUT_OF_MEMORY;

            // Before proceeding, eagerly create any same-compartment security wrappers
            // that the object might have. This forces us to take the 'WithWrapper' path
            // while transplanting that handles this stuff correctly.
            {
                JSAutoEnterCompartment innerAC;
                if (!innerAC.enter(ccx, aOldScope->GetGlobalJSObject()) ||
                    !wrapper->GetSameCompartmentSecurityWrapper(ccx))
                    return NS_ERROR_FAILURE;
            }

            // Null out the private of the JS reflector. If we don't, we'll end up
            // with two JS objects with the same WN in their private slot, and both
            // will try to delete it during finalization. The one in this
            // compartment will actually go away quite soon, because we swap() it
            // with another object during the transplant and let that object die.
            //
            // NB: It's important to do this _after_ copying the properties to
            // propertyHolder. Otherwise, an object with |foo.x === foo| will
            // crash when JS_CopyPropertiesFrom tries to call wrap() on foo.x.
            JS_SetPrivate(flat, nsnull);

            JSObject *ww = wrapper->GetWrapper();
            if (ww) {
                JSObject *newwrapper;
                MOZ_ASSERT(!xpc::WrapperFactory::IsComponentsObject(flat), 
                           "Components object should never get here");
                if (xpc::WrapperFactory::IsLocationObject(flat)) {
                    newwrapper = xpc::WrapperFactory::WrapLocationObject(ccx, newobj);
                    if (!newwrapper)
                        return NS_ERROR_FAILURE;
                } else {
                    NS_ASSERTION(wrapper->NeedsSOW(), "weird wrapper wrapper");
                    newwrapper = xpc::WrapperFactory::WrapSOWObject(ccx, newobj);
                    if (!newwrapper)
                        return NS_ERROR_FAILURE;
                }

                // Ok, now we do the special object-plus-wrapper transplant.
                //
                // This is some pretty serious brain surgery.
                //
                // In the case where we wrap a Location object from a same-
                // origin compartment, we actually want our cross-compartment
                // wrapper to point to the same-compartment wrapper in the
                // other compartment. This double-wrapping allows expandos to
                // be shared. So our wrapping callback (in WrapperFactory.cpp)
                // calls XPCWrappedNative::GetSameCompartmentSecurityWrapper
                // before wrapping same-origin Location objects.
                //
                // This normally works fine, but gets tricky here.
                // js_TransplantObjectWithWrapper needs to update the old
                // same-compartment security wrapper to be a cross-compartment
                // wrapper to the newly transplanted object. So it needs to go
                // through the aforementioned double-wrapping mechanism.
                // But during the call, things aren't really in a consistent
                // state, because mFlatJSObject hasn't yet been updated to
                // point to the object in the new compartment.
                //
                // So we need to cache the new same-compartment security
                // wrapper on the XPCWN before the call, so that
                // GetSameCompartmentSecurityWrapper can return early before
                // getting confused. Hold your breath.
                JSObject *wwsaved = ww;
                wrapper->SetWrapper(newwrapper);
                ww = js_TransplantObjectWithWrapper(ccx, flat, ww, newobj,
                                                    newwrapper);
                if (!ww) {
                    wrapper->SetWrapper(wwsaved);
                    return NS_ERROR_FAILURE;
                }

                flat = newobj;
            } else {
                flat = JS_TransplantObject(ccx, flat, newobj);
                if (!flat)
                    return NS_ERROR_FAILURE;
            }

            wrapper->mFlatJSObject = flat;
            if (cache)
                cache->SetWrapper(flat);
            if (!JS_CopyPropertiesFrom(ccx, flat, propertyHolder))
                return NS_ERROR_FAILURE;
        } else {
            JS_SetReservedSlot(flat, 0,
                               PRIVATE_TO_JSVAL(newProto.get()));
            if (!JS_SetPrototype(ccx, flat, newProto->GetJSProtoObject())) {
                // this is bad, very bad
                JS_SetReservedSlot(flat, 0, JSVAL_NULL);
                NS_ERROR("JS_SetPrototype failed");
                return NS_ERROR_FAILURE;
            }
        }
    }

    // Now we can just fix up the parent and return the wrapper

    if (aNewParent) {
        if (!JS_SetParent(ccx, flat, aNewParent))
            return NS_ERROR_FAILURE;

        JSObject *nw;
        if (wrapper &&
            (nw = wrapper->GetWrapper()) &&
            !JS_SetParent(ccx, nw, JS_GetGlobalForObject(ccx, aNewParent))) {
            return NS_ERROR_FAILURE;
        }
    }

    *aWrapper = nsnull;
    wrapper.swap(*aWrapper);

    return NS_OK;
}

#define IS_TEAROFF_CLASS(clazz)                                               \
          ((clazz) == &XPC_WN_Tearoff_JSClass)

// static
XPCWrappedNative*
XPCWrappedNative::GetWrappedNativeOfJSObject(JSContext* cx,
                                             JSObject* obj,
                                             JSObject* funobj,
                                             JSObject** pobj2,
                                             XPCWrappedNativeTearOff** pTearOff)
{
    NS_PRECONDITION(obj, "bad param");

    // fubobj must be null if called without cx.
    NS_PRECONDITION(cx || !funobj, "bad param");

    // *pTeaorOff must be null if pTearOff is given
    NS_PRECONDITION(!pTearOff || !*pTearOff, "bad param");

    JSObject* cur;

    XPCWrappedNativeProto* proto = nsnull;
    nsIClassInfo* protoClassInfo = nsnull;

    // If we were passed a function object then we need to find the correct
    // wrapper out of those that might be in the callee obj's proto chain.

    if (funobj) {
        JSObject* funObjParent = js::UnwrapObject(js::GetObjectParent(funobj));
        funObjParent = JS_ObjectToInnerObject(cx, funObjParent);
        NS_ASSERTION(funObjParent, "funobj has no parent");

        js::Class* funObjParentClass = js::GetObjectClass(funObjParent);

        if (IS_PROTO_CLASS(funObjParentClass)) {
            NS_ASSERTION(js::GetObjectParent(funObjParent), "funobj's parent (proto) is global");
            proto = (XPCWrappedNativeProto*) js::GetObjectPrivate(funObjParent);
            if (proto)
                protoClassInfo = proto->GetClassInfo();
        } else if (IS_WRAPPER_CLASS(funObjParentClass)) {
            cur = funObjParent;
            goto return_wrapper;
        } else if (IS_TEAROFF_CLASS(funObjParentClass)) {
            NS_ASSERTION(js::GetObjectParent(funObjParent), "funobj's parent (tearoff) is global");
            cur = funObjParent;
            goto return_tearoff;
        } else {
            NS_ERROR("function object has parent of unknown class!");
            return nsnull;
        }
    }

  restart:
    for (cur = obj; cur; cur = js::GetObjectProto(cur)) {
        // this is on two lines to make the compiler happy given the goto.
        js::Class* clazz;
        clazz = js::GetObjectClass(cur);

        if (IS_WRAPPER_CLASS(clazz)) {
return_wrapper:
            JSBool isWN = IS_WN_WRAPPER_OBJECT(cur);
            XPCWrappedNative* wrapper =
                isWN ? (XPCWrappedNative*) js::GetObjectPrivate(cur) : nsnull;
            if (proto) {
                XPCWrappedNativeProto* wrapper_proto =
                    isWN ? wrapper->GetProto() : GetSlimWrapperProto(cur);
                if (proto != wrapper_proto &&
                    (!protoClassInfo || !wrapper_proto ||
                     protoClassInfo != wrapper_proto->GetClassInfo()))
                    continue;
            }
            if (pobj2)
                *pobj2 = isWN ? nsnull : cur;
            return wrapper;
        }

        if (IS_TEAROFF_CLASS(clazz)) {
return_tearoff:
            XPCWrappedNative* wrapper =
                (XPCWrappedNative*) js::GetObjectPrivate(js::GetObjectParent(cur));
            if (proto && proto != wrapper->GetProto() &&
                (proto->GetScope() != wrapper->GetScope() ||
                 !protoClassInfo || !wrapper->GetProto() ||
                 protoClassInfo != wrapper->GetProto()->GetClassInfo()))
                continue;
            if (pobj2)
                *pobj2 = nsnull;
            XPCWrappedNativeTearOff* to = (XPCWrappedNativeTearOff*) js::GetObjectPrivate(cur);
            if (!to)
                return nsnull;
            if (pTearOff)
                *pTearOff = to;
            return wrapper;
        }

        // Unwrap any wrapper wrappers.
        JSObject *unsafeObj = cx
                              ? XPCWrapper::Unwrap(cx, cur, /* stopAtOuter = */ false)
                              : js::UnwrapObject(cur, /* stopAtOuter = */ false);
        if (unsafeObj) {
            obj = unsafeObj;
            goto restart;
        }
    }

    if (pobj2)
        *pobj2 = nsnull;
    return nsnull;
}

JSBool
XPCWrappedNative::ExtendSet(XPCCallContext& ccx, XPCNativeInterface* aInterface)
{
    // This is only called while locked (during XPCWrappedNative::FindTearOff).

    if (!mSet->HasInterface(aInterface)) {
        AutoMarkingNativeSetPtr newSet(ccx);
        newSet = XPCNativeSet::GetNewOrUsed(ccx, mSet, aInterface,
                                            mSet->GetInterfaceCount());
        if (!newSet)
            return false;

        mSet = newSet;

        DEBUG_ReportShadowedMembers(newSet, this, GetProto());
    }
    return true;
}

XPCWrappedNativeTearOff*
XPCWrappedNative::LocateTearOff(XPCCallContext& ccx,
                                XPCNativeInterface* aInterface)
{
    XPCAutoLock al(GetLock()); // hold the lock throughout

    for (XPCWrappedNativeTearOffChunk* chunk = &mFirstChunk;
         chunk != nsnull;
         chunk = chunk->mNextChunk) {
        XPCWrappedNativeTearOff* tearOff = chunk->mTearOffs;
        XPCWrappedNativeTearOff* const end = tearOff +
            XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK;
        for (tearOff = chunk->mTearOffs;
             tearOff < end;
             tearOff++) {
            if (tearOff->GetInterface() == aInterface) {
                return tearOff;
            }
        }
    }
    return nsnull;
}

XPCWrappedNativeTearOff*
XPCWrappedNative::FindTearOff(XPCCallContext& ccx,
                              XPCNativeInterface* aInterface,
                              JSBool needJSObject /* = false */,
                              nsresult* pError /* = nsnull */)
{
    XPCAutoLock al(GetLock()); // hold the lock throughout

    nsresult rv = NS_OK;
    XPCWrappedNativeTearOff* to;
    XPCWrappedNativeTearOff* firstAvailable = nsnull;

    XPCWrappedNativeTearOffChunk* lastChunk;
    XPCWrappedNativeTearOffChunk* chunk;
    for (lastChunk = chunk = &mFirstChunk;
         chunk;
         lastChunk = chunk, chunk = chunk->mNextChunk) {
        to = chunk->mTearOffs;
        XPCWrappedNativeTearOff* const end = chunk->mTearOffs +
            XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK;
        for (to = chunk->mTearOffs;
             to < end;
             to++) {
            if (to->GetInterface() == aInterface) {
                if (needJSObject && !to->GetJSObjectPreserveColor()) {
                    AutoMarkingWrappedNativeTearOffPtr tearoff(ccx, to);
                    JSBool ok = InitTearOffJSObject(ccx, to);
                    // During shutdown, we don't sweep tearoffs.  So make sure
                    // to unmark manually in case the auto-marker marked us.
                    // We shouldn't ever be getting here _during_ our
                    // Mark/Sweep cycle, so this should be safe.
                    to->Unmark();
                    if (!ok) {
                        to = nsnull;
                        rv = NS_ERROR_OUT_OF_MEMORY;
                    }
                }
                goto return_result;
            }
            if (!firstAvailable && to->IsAvailable())
                firstAvailable = to;
        }
    }

    to = firstAvailable;

    if (!to) {
        XPCWrappedNativeTearOffChunk* newChunk =
            new XPCWrappedNativeTearOffChunk();
        if (!newChunk) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto return_result;
        }
        lastChunk->mNextChunk = newChunk;
        to = newChunk->mTearOffs;
    }

    {
        // Scope keeps |tearoff| from leaking across the return_result: label
        AutoMarkingWrappedNativeTearOffPtr tearoff(ccx, to);
        rv = InitTearOff(ccx, to, aInterface, needJSObject);
        // During shutdown, we don't sweep tearoffs.  So make sure to unmark
        // manually in case the auto-marker marked us.  We shouldn't ever be
        // getting here _during_ our Mark/Sweep cycle, so this should be safe.
        to->Unmark();
        if (NS_FAILED(rv))
            to = nsnull;
    }

return_result:

    if (pError)
        *pError = rv;
    return to;
}

nsresult
XPCWrappedNative::InitTearOff(XPCCallContext& ccx,
                              XPCWrappedNativeTearOff* aTearOff,
                              XPCNativeInterface* aInterface,
                              JSBool needJSObject)
{
    // This is only called while locked (during XPCWrappedNative::FindTearOff).

    // Determine if the object really does this interface...

    const nsIID* iid = aInterface->GetIID();
    nsISupports* identity = GetIdentityObject();
    nsISupports* obj;

    // If the scriptable helper forbids us from reflecting additional
    // interfaces, then don't even try the QI, just fail.
    if (mScriptableInfo &&
        mScriptableInfo->GetFlags().ClassInfoInterfacesOnly() &&
        !mSet->HasInterface(aInterface) &&
        !mSet->HasInterfaceWithAncestor(aInterface)) {
        return NS_ERROR_NO_INTERFACE;
    }

    // We are about to call out to unlock and other code.
    // So protect our intended tearoff.

    aTearOff->SetReserved();

    {   // scoped *un*lock
        XPCAutoUnlock unlock(GetLock());

        if (NS_FAILED(identity->QueryInterface(*iid, (void**)&obj)) || !obj) {
            aTearOff->SetInterface(nsnull);
            return NS_ERROR_NO_INTERFACE;
        }

        // Guard against trying to build a tearoff for a shared nsIClassInfo.
        if (iid->Equals(NS_GET_IID(nsIClassInfo))) {
            nsCOMPtr<nsISupports> alternate_identity(do_QueryInterface(obj));
            if (alternate_identity.get() != identity) {
                NS_RELEASE(obj);
                aTearOff->SetInterface(nsnull);
                return NS_ERROR_NO_INTERFACE;
            }
        }

        // Guard against trying to build a tearoff for an interface that is
        // aggregated and is implemented as a nsIXPConnectWrappedJS using this
        // self-same JSObject. The XBL system does this. If we mutate the set
        // of this wrapper then we will shadow the method that XBL has added to
        // the JSObject that it has inserted in the JS proto chain between our
        // JSObject and our XPCWrappedNativeProto's JSObject. If we let this
        // set mutation happen then the interface's methods will be added to
        // our JSObject, but calls on those methods will get routed up to
        // native code and into the wrappedJS - which will do a method lookup
        // on *our* JSObject and find the same method and make another call
        // into an infinite loop.
        // see: http://bugzilla.mozilla.org/show_bug.cgi?id=96725

        // The code in this block also does a check for the double wrapped
        // nsIPropertyBag case.

        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(obj));
        if (wrappedJS) {
            JSObject* jso = nsnull;
            if (NS_SUCCEEDED(wrappedJS->GetJSObject(&jso)) &&
                jso == mFlatJSObject) {
                // The implementing JSObject is the same as ours! Just say OK
                // without actually extending the set.
                //
                // XXX It is a little cheesy to have FindTearOff return an
                // 'empty' tearoff. But this is the centralized place to do the
                // QI activities on the underlying object. *And* most caller to
                // FindTearOff only look for a non-null result and ignore the
                // actual tearoff returned. The only callers that do use the
                // returned tearoff make sure to check for either a non-null
                // JSObject or a matching Interface before proceeding.
                // I think we can get away with this bit of ugliness.

#ifdef DEBUG_xpc_hacker
                {
                    // I want to make sure this only happens in xbl-like cases.
                    // So, some debug code to verify that there is at least
                    // *some* object between our JSObject and its inital proto.
                    // XXX This is a pretty funky test. Someone might hack it
                    // a bit if false positives start showing up. Note that
                    // this is only going to run for the few people in the
                    // DEBUG_xpc_hacker list.
                    if (HasProto()) {
                        JSObject* proto  = nsnull;
                        JSObject* our_proto = GetProto()->GetJSProtoObject();

                        proto = jso->getProto();

                        NS_ASSERTION(proto && proto != our_proto,
                                     "!!! xpconnect/xbl check - wrapper has no special proto");

                        bool found_our_proto = false;
                        while (proto && !found_our_proto) {
                            proto = proto->getProto();

                            found_our_proto = proto == our_proto;
                        }

                        NS_ASSERTION(found_our_proto,
                                     "!!! xpconnect/xbl check - wrapper has extra proto");
                    } else {
                        NS_WARNING("!!! xpconnect/xbl check - wrapper has no proto");
                    }
                }
#endif
                NS_RELEASE(obj);
                aTearOff->SetInterface(nsnull);
                return NS_OK;
            }

            // Decide whether or not to expose nsIPropertyBag to calling
            // JS code in the double wrapped case.
            //
            // Our rule here is that when JSObjects are double wrapped and
            // exposed to other JSObjects then the nsIPropertyBag interface
            // is only exposed on an 'opt-in' basis; i.e. if the underlying
            // JSObject wants other JSObjects to be able to see this interface
            // then it must implement QueryInterface and not throw an exception
            // when asked for nsIPropertyBag. It need not actually *implement*
            // nsIPropertyBag - xpconnect will do that work.

            nsXPCWrappedJSClass* clazz;
            if (iid->Equals(NS_GET_IID(nsIPropertyBag)) && jso &&
                NS_SUCCEEDED(nsXPCWrappedJSClass::GetNewOrUsed(ccx,*iid,&clazz))&&
                clazz) {
                JSObject* answer =
                    clazz->CallQueryInterfaceOnJSObject(ccx, jso, *iid);
                NS_RELEASE(clazz);
                if (!answer) {
                    NS_RELEASE(obj);
                    aTearOff->SetInterface(nsnull);
                    return NS_ERROR_NO_INTERFACE;
                }
            }
        }

        nsIXPCSecurityManager* sm;
           sm = ccx.GetXPCContext()->GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_CREATE_WRAPPER);
        if (sm && NS_FAILED(sm->
                            CanCreateWrapper(ccx, *iid, identity,
                                             GetClassInfo(), GetSecurityInfoAddr()))) {
            // the security manager vetoed. It should have set an exception.
            NS_RELEASE(obj);
            aTearOff->SetInterface(nsnull);
            return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
        }
    }
    // We are relocked from here on...

    // If this is not already in our set we need to extend our set.
    // Note: we do not cache the result of the previous call to HasInterface()
    // because we unlocked and called out in the interim and the result of the
    // previous call might not be correct anymore.

    if (!mSet->HasInterface(aInterface) && !ExtendSet(ccx, aInterface)) {
        NS_RELEASE(obj);
        aTearOff->SetInterface(nsnull);
        return NS_ERROR_NO_INTERFACE;
    }

    aTearOff->SetInterface(aInterface);
    aTearOff->SetNative(obj);
    if (needJSObject && !InitTearOffJSObject(ccx, aTearOff))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

JSBool
XPCWrappedNative::InitTearOffJSObject(XPCCallContext& ccx,
                                      XPCWrappedNativeTearOff* to)
{
    // This is only called while locked (during XPCWrappedNative::FindTearOff).

    JSObject* obj =
        xpc_NewSystemInheritingJSObject(ccx, Jsvalify(&XPC_WN_Tearoff_JSClass),
                                        GetScope()->GetPrototypeJSObject(),
                                        false, mFlatJSObject);

    if (!obj)
        return false;

    JS_SetPrivate(obj, to);
    to->SetJSObject(obj);
    return true;
}

JSObject*
XPCWrappedNative::GetSameCompartmentSecurityWrapper(JSContext *cx)
{
    // Grab the current state of affairs.
    JSObject *flat = GetFlatJSObject();
    JSObject *wrapper = GetWrapper();

    // If we already have a wrapper, it must be what we want.
    //
    // NB: This must come before anything below because of some trickiness
    // with brain transplants. See the "pretty serious brain surgery" comment
    // in ReparentWrapperIfFound.
    if (wrapper)
        return wrapper;

    // Chrome callers don't need same-compartment security wrappers.
    JSCompartment *cxCompartment = js::GetContextCompartment(cx);
    MOZ_ASSERT(cxCompartment == js::GetObjectCompartment(flat));
    if (xpc::AccessCheck::isChrome(cxCompartment)) {
        MOZ_ASSERT(wrapper == NULL);
        return flat;
    }

    // Check the possibilities. Note that we need to check for null in each
    // case in order to distinguish between the 'no need for wrapper' and
    // 'wrapping failed' cases.
    if (xpc::WrapperFactory::IsLocationObject(flat)) {
        wrapper = xpc::WrapperFactory::WrapLocationObject(cx, flat);
        if (!wrapper)
            return NULL;
    } else if (NeedsSOW()) {
        wrapper = xpc::WrapperFactory::WrapSOWObject(cx, flat);
        if (!wrapper)
            return NULL;
    } else if (xpc::WrapperFactory::IsComponentsObject(flat)) {
        wrapper = xpc::WrapperFactory::WrapComponentsObject(cx, flat);
        if (!wrapper)
            return NULL;
    }

    // If we made a wrapper, cache it and return it.
    if (wrapper) {
        SetWrapper(wrapper);
        return wrapper;
    }

    // Otherwise, just return the bare JS reflection.
    return flat;
}

/***************************************************************************/

static JSBool Throw(unsigned errNum, XPCCallContext& ccx)
{
    XPCThrower::Throw(errNum, ccx);
    return false;
}

/***************************************************************************/

class CallMethodHelper
{
    XPCCallContext& mCallContext;
    nsIInterfaceInfo* const mIFaceInfo;
    const nsXPTMethodInfo* mMethodInfo;
    nsISupports* const mCallee;
    const uint16_t mVTableIndex;
    const jsid mIdxValueId;

    nsAutoTArray<nsXPTCVariant, 8> mDispatchParams;
    uint8_t mJSContextIndex; // TODO make const
    uint8_t mOptArgcIndex; // TODO make const

    jsval* const mArgv;
    const PRUint32 mArgc;

    JS_ALWAYS_INLINE JSBool
    GetArraySizeFromParam(uint8_t paramIndex, uint32_t* result) const;

    JS_ALWAYS_INLINE JSBool
    GetInterfaceTypeFromParam(uint8_t paramIndex,
                              const nsXPTType& datum_type,
                              nsID* result) const;

    JS_ALWAYS_INLINE JSBool
    GetOutParamSource(uint8_t paramIndex, jsval* srcp) const;

    JS_ALWAYS_INLINE JSBool
    GatherAndConvertResults();

    JS_ALWAYS_INLINE JSBool
    QueryInterfaceFastPath() const;

    nsXPTCVariant*
    GetDispatchParam(uint8_t paramIndex)
    {
        if (paramIndex >= mJSContextIndex)
            paramIndex += 1;
        if (paramIndex >= mOptArgcIndex)
            paramIndex += 1;
        return &mDispatchParams[paramIndex];
    }
    const nsXPTCVariant*
    GetDispatchParam(uint8_t paramIndex) const
    {
        return const_cast<CallMethodHelper*>(this)->GetDispatchParam(paramIndex);
    }

    JS_ALWAYS_INLINE JSBool InitializeDispatchParams();

    JS_ALWAYS_INLINE JSBool ConvertIndependentParams(JSBool* foundDependentParam);
    JS_ALWAYS_INLINE JSBool ConvertIndependentParam(uint8_t i);
    JS_ALWAYS_INLINE JSBool ConvertDependentParams();
    JS_ALWAYS_INLINE JSBool ConvertDependentParam(uint8_t i);

    JS_ALWAYS_INLINE void CleanupParam(nsXPTCMiniVariant& param, nsXPTType& type);

    JS_ALWAYS_INLINE JSBool HandleDipperParam(nsXPTCVariant* dp,
                                              const nsXPTParamInfo& paramInfo);

    JS_ALWAYS_INLINE nsresult Invoke();

public:

    CallMethodHelper(XPCCallContext& ccx)
        : mCallContext(ccx)
        , mIFaceInfo(ccx.GetInterface()->GetInterfaceInfo())
        , mMethodInfo(nsnull)
        , mCallee(ccx.GetTearOff()->GetNative())
        , mVTableIndex(ccx.GetMethodIndex())
        , mIdxValueId(ccx.GetRuntime()->GetStringID(XPCJSRuntime::IDX_VALUE))
        , mJSContextIndex(PR_UINT8_MAX)
        , mOptArgcIndex(PR_UINT8_MAX)
        , mArgv(ccx.GetArgv())
        , mArgc(ccx.GetArgc())

    {
        // Success checked later.
        mIFaceInfo->GetMethodInfo(mVTableIndex, &mMethodInfo);
    }

    ~CallMethodHelper();

    JS_ALWAYS_INLINE JSBool Call();

};

// static
NS_SUPPRESS_STACK_CHECK JSBool
XPCWrappedNative::CallMethod(XPCCallContext& ccx,
                             CallMode mode /*= CALL_METHOD */)
{
    XPCContext* xpcc = ccx.GetXPCContext();
    NS_ASSERTION(xpcc->CallerTypeIsJavaScript(),
                 "Native caller for XPCWrappedNative::CallMethod?");

    nsresult rv = ccx.CanCallNow();
    if (NS_FAILED(rv)) {
        // If the security manager is complaining then this is not really an
        // internal error in xpconnect. So, no reason to botch the assertion.
        NS_ASSERTION(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO,
                     "hmm? CanCallNow failed in XPCWrappedNative::CallMethod. "
                     "We are finding out about this late!");
        return Throw(rv, ccx);
    }

    DEBUG_TrackWrapperCall(ccx.GetWrapper(), mode);

    // set up the method index and do the security check if needed

    PRUint32 secFlag;
    PRUint32 secAction;

    switch (mode) {
        case CALL_METHOD:
            secFlag   = nsIXPCSecurityManager::HOOK_CALL_METHOD;
            secAction = nsIXPCSecurityManager::ACCESS_CALL_METHOD;
            break;
        case CALL_GETTER:
            secFlag   = nsIXPCSecurityManager::HOOK_GET_PROPERTY;
            secAction = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
            break;
        case CALL_SETTER:
            secFlag   = nsIXPCSecurityManager::HOOK_SET_PROPERTY;
            secAction = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
            break;
        default:
            NS_ERROR("bad value");
            return false;
    }

    nsIXPCSecurityManager* sm =
        xpcc->GetAppropriateSecurityManager(secFlag);
    if (sm && NS_FAILED(sm->CanAccess(secAction, &ccx, ccx,
                                      ccx.GetFlattenedJSObject(),
                                      ccx.GetWrapper()->GetIdentityObject(),
                                      ccx.GetWrapper()->GetClassInfo(),
                                      ccx.GetMember()->GetName(),
                                      ccx.GetWrapper()->GetSecurityInfoAddr()))) {
        // the security manager vetoed. It should have set an exception.
        return false;
    }

    return CallMethodHelper(ccx).Call();
}

JSBool
CallMethodHelper::Call()
{
    mCallContext.SetRetVal(JSVAL_VOID);

    mCallContext.GetThreadData()->SetException(nsnull);
    mCallContext.GetXPCContext()->SetLastResult(NS_ERROR_UNEXPECTED);

    if (mVTableIndex == 0) {
        return QueryInterfaceFastPath();
    }

    if (!mMethodInfo) {
        Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, mCallContext);
        return false;
    }

    if (!InitializeDispatchParams())
        return false;

    // Iterate through the params doing conversions of independent params only.
    // When we later convert the dependent params (if any) we will know that
    // the params upon which they depend will have already been converted -
    // regardless of ordering.
    JSBool foundDependentParam = false;
    if (!ConvertIndependentParams(&foundDependentParam))
        return false;

    if (foundDependentParam && !ConvertDependentParams())
        return false;

    nsresult invokeResult = Invoke();

    mCallContext.GetXPCContext()->SetLastResult(invokeResult);

    if (NS_FAILED(invokeResult)) {
        ThrowBadResult(invokeResult, mCallContext);
        return false;
    }

    if (JS_IsExceptionPending(mCallContext)) {
        return false;
    }

    return GatherAndConvertResults();
}

CallMethodHelper::~CallMethodHelper()
{
    uint8_t paramCount = mMethodInfo->GetParamCount();
    if (mDispatchParams.Length()) {
        for (uint8_t i = 0; i < paramCount; i++) {
            nsXPTCVariant* dp = GetDispatchParam(i);
            const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);

            if (paramInfo.GetType().IsArray()) {
                void* p = dp->val.p;
                if (!p)
                    continue;

                // Clean up the array contents if necessary.
                if (dp->DoesValNeedCleanup()) {
                    // We need some basic information to properly destroy the array.
                    uint32_t array_count = 0;
                    nsXPTType datum_type;
                    if (!GetArraySizeFromParam(i, &array_count) ||
                        !NS_SUCCEEDED(mIFaceInfo->GetTypeForParam(mVTableIndex,
                                                                  &paramInfo,
                                                                  1, &datum_type))) {
                        // XXXbholley - I'm not convinced that the above calls will
                        // ever fail.
                        NS_ERROR("failed to get array information, we'll leak here");
                        continue;
                    }

                    // Loop over the array contents. For each one, we create a
                    // dummy 'val' and pass it to the cleanup helper.
                    for (uint32_t k = 0; k < array_count; k++) {
                        nsXPTCMiniVariant v;
                        v.val.p = static_cast<void**>(p)[k];
                        CleanupParam(v, datum_type);
                    }
                }

                // always free the array itself
                nsMemory::Free(p);
            } else {
                // Clean up single parameters (if requested).
                if (dp->DoesValNeedCleanup())
                    CleanupParam(*dp, dp->type);
            }
        }
    }

}

JSBool
CallMethodHelper::GetArraySizeFromParam(uint8_t paramIndex,
                                        uint32_t* result) const
{
    nsresult rv;
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(paramIndex);

    // TODO fixup the various exceptions that are thrown

    rv = mIFaceInfo->GetSizeIsArgNumberForParam(mVTableIndex, &paramInfo, 0, &paramIndex);
    if (NS_FAILED(rv))
        return Throw(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, mCallContext);

    *result = GetDispatchParam(paramIndex)->val.u32;

    return true;
}

JSBool
CallMethodHelper::GetInterfaceTypeFromParam(uint8_t paramIndex,
                                            const nsXPTType& datum_type,
                                            nsID* result) const
{
    nsresult rv;
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(paramIndex);
    uint8_t tag = datum_type.TagPart();

    // TODO fixup the various exceptions that are thrown

    if (tag == nsXPTType::T_INTERFACE) {
        rv = mIFaceInfo->GetIIDForParamNoAlloc(mVTableIndex, &paramInfo, result);
        if (NS_FAILED(rv))
            return ThrowBadParam(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,
                                 paramIndex, mCallContext);
    } else if (tag == nsXPTType::T_INTERFACE_IS) {
        rv = mIFaceInfo->GetInterfaceIsArgNumberForParam(mVTableIndex, &paramInfo,
                                                         &paramIndex);
        if (NS_FAILED(rv))
            return Throw(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, mCallContext);

        nsID* p = (nsID*) GetDispatchParam(paramIndex)->val.p;
        if (!p)
            return ThrowBadParam(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,
                                 paramIndex, mCallContext);
        *result = *p;
    }
    return true;
}

JSBool
CallMethodHelper::GetOutParamSource(uint8_t paramIndex, jsval* srcp) const
{
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(paramIndex);

    if ((paramInfo.IsOut() || paramInfo.IsDipper()) &&
        !paramInfo.IsRetval()) {
        NS_ASSERTION(paramIndex < mArgc || paramInfo.IsOptional(),
                     "Expected either enough arguments or an optional argument");
        jsval arg = paramIndex < mArgc ? mArgv[paramIndex] : JSVAL_NULL;
        if (paramIndex < mArgc &&
            (JSVAL_IS_PRIMITIVE(arg) ||
             !JS_GetPropertyById(mCallContext,
                                 JSVAL_TO_OBJECT(arg),
                                 mIdxValueId,
                                 srcp))) {
            // Explicitly passed in unusable value for out param.  Note
            // that if i >= mArgc we already know that |arg| is JSVAL_NULL,
            // and that's ok.
            ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, paramIndex,
                          mCallContext);
            return false;
        }
    }

    return true;
}

JSBool
CallMethodHelper::GatherAndConvertResults()
{
    // now we iterate through the native params to gather and convert results
    uint8_t paramCount = mMethodInfo->GetParamCount();
    for (uint8_t i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
        if (!paramInfo.IsOut() && !paramInfo.IsDipper())
            continue;

        const nsXPTType& type = paramInfo.GetType();
        nsXPTCVariant* dp = GetDispatchParam(i);
        jsval v = JSVAL_NULL;
        AUTO_MARK_JSVAL(mCallContext, &v);
        uint32_t array_count = 0;
        nsXPTType datum_type;
        bool isArray = type.IsArray();
        bool isSizedString = isArray ?
                false :
                type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;

        if (isArray) {
            if (NS_FAILED(mIFaceInfo->GetTypeForParam(mVTableIndex, &paramInfo, 1,
                                                      &datum_type))) {
                Throw(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, mCallContext);
                return false;
            }
        } else
            datum_type = type;

        if (isArray || isSizedString) {
            if (!GetArraySizeFromParam(i, &array_count))
                return false;
        }

        nsID param_iid;
        if (datum_type.IsInterfacePointer() &&
            !GetInterfaceTypeFromParam(i, datum_type, &param_iid))
            return false;

        nsresult err;
        if (isArray) {
            XPCLazyCallContext lccx(mCallContext);
            if (!XPCConvert::NativeArray2JS(lccx, &v, (const void**)&dp->val,
                                            datum_type, &param_iid,
                                            array_count, &err)) {
                // XXX need exception scheme for arrays to indicate bad element
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        } else if (isSizedString) {
            if (!XPCConvert::NativeStringWithSize2JS(mCallContext, &v,
                                                     (const void*)&dp->val,
                                                     datum_type,
                                                     array_count, &err)) {
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        } else {
            if (!XPCConvert::NativeData2JS(mCallContext, &v, &dp->val, datum_type,
                                           &param_iid, &err)) {
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        }

        if (paramInfo.IsRetval()) {
            mCallContext.SetRetVal(v);
        } else if (i < mArgc) {
            // we actually assured this before doing the invoke
            NS_ASSERTION(mArgv[i].isObject(), "out var is not object");
            if (!JS_SetPropertyById(mCallContext,
                                    &mArgv[i].toObject(),
                                    mIdxValueId, &v)) {
                ThrowBadParam(NS_ERROR_XPC_CANT_SET_OUT_VAL, i, mCallContext);
                return false;
            }
        } else {
            NS_ASSERTION(paramInfo.IsOptional(),
                         "Expected either enough arguments or an optional argument");
        }
    }

    return true;
}

JSBool
CallMethodHelper::QueryInterfaceFastPath() const
{
    NS_ASSERTION(mVTableIndex == 0,
                 "Using the QI fast-path for a method other than QueryInterface");

    if (mArgc < 1) {
        Throw(NS_ERROR_XPC_NOT_ENOUGH_ARGS, mCallContext);
        return false;
    }
    
    if (!mArgv[0].isObject()) {
        ThrowBadParam(NS_ERROR_XPC_BAD_CONVERT_JS, 0, mCallContext);
        return false;
    }

    const nsID* iid = xpc_JSObjectToID(mCallContext, &mArgv[0].toObject());
    if (!iid) {
        ThrowBadParam(NS_ERROR_XPC_BAD_CONVERT_JS, 0, mCallContext);
        return false;
    }

    nsresult invokeResult;
    nsISupports* qiresult = nsnull;
    if (XPCPerThreadData::IsMainThread(mCallContext)) {
        invokeResult = mCallee->QueryInterface(*iid, (void**) &qiresult);
    } else {
        JSAutoSuspendRequest suspended(mCallContext);
        invokeResult = mCallee->QueryInterface(*iid, (void**) &qiresult);
    }

    mCallContext.GetXPCContext()->SetLastResult(invokeResult);

    if (NS_FAILED(invokeResult)) {
        ThrowBadResult(invokeResult, mCallContext);
        return false;
    }

    jsval v = JSVAL_NULL;
    unsigned err;
    JSBool success =
        XPCConvert::NativeData2JS(mCallContext, &v, &qiresult,
                                  nsXPTType::T_INTERFACE_IS,
                                  iid, &err);
    NS_IF_RELEASE(qiresult);

    if (!success) {
        ThrowBadParam(err, 0, mCallContext);
        return false;
    }

    mCallContext.SetRetVal(v);
    return true;
}

JSBool
CallMethodHelper::InitializeDispatchParams()
{
    const uint8_t wantsOptArgc = mMethodInfo->WantsOptArgc() ? 1 : 0;
    const uint8_t wantsJSContext = mMethodInfo->WantsContext() ? 1 : 0;
    const uint8_t paramCount = mMethodInfo->GetParamCount();
    uint8_t requiredArgs = paramCount;
    uint8_t hasRetval = 0;

    // XXX ASSUMES that retval is last arg. The xpidl compiler ensures this.
    if (paramCount && mMethodInfo->GetParam(paramCount-1).IsRetval()) {
        hasRetval = 1;
        requiredArgs--;
    }

    if (mArgc < requiredArgs || wantsOptArgc) {
        if (wantsOptArgc)
            mOptArgcIndex = requiredArgs;

        // skip over any optional arguments
        while (requiredArgs && mMethodInfo->GetParam(requiredArgs-1).IsOptional())
            requiredArgs--;

        if (mArgc < requiredArgs) {
            Throw(NS_ERROR_XPC_NOT_ENOUGH_ARGS, mCallContext);
            return false;
        }
    }

    if (wantsJSContext) {
        if (wantsOptArgc)
            // Need to bump mOptArgcIndex up one here.
            mJSContextIndex = mOptArgcIndex++;
        else if (mMethodInfo->IsSetter() || mMethodInfo->IsGetter())
            // For attributes, we always put the JSContext* first.
            mJSContextIndex = 0;
        else
            mJSContextIndex = paramCount - hasRetval;
    }

    // iterate through the params to clear flags (for safe cleanup later)
    for (uint8_t i = 0; i < paramCount + wantsJSContext + wantsOptArgc; i++) {
        nsXPTCVariant* dp = mDispatchParams.AppendElement();
        dp->ClearFlags();
        dp->val.p = nsnull;
    }

    // Fill in the JSContext argument
    if (wantsJSContext) {
        nsXPTCVariant* dp = &mDispatchParams[mJSContextIndex];
        dp->type = nsXPTType::T_VOID;
        dp->val.p = mCallContext;
    }

    // Fill in the optional_argc argument
    if (wantsOptArgc) {
        nsXPTCVariant* dp = &mDispatchParams[mOptArgcIndex];
        dp->type = nsXPTType::T_U8;
        dp->val.u8 = NS_MIN<PRUint32>(mArgc, paramCount) - requiredArgs;
    }

    return true;
}

JSBool
CallMethodHelper::ConvertIndependentParams(JSBool* foundDependentParam)
{
    const uint8_t paramCount = mMethodInfo->GetParamCount();
    for (uint8_t i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);

        if (paramInfo.GetType().IsDependent())
            *foundDependentParam = true;
        else if (!ConvertIndependentParam(i))
            return false;

    }

    return true;
}

JSBool
CallMethodHelper::ConvertIndependentParam(uint8_t i)
{
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
    const nsXPTType& type = paramInfo.GetType();
    uint8_t type_tag = type.TagPart();
    nsXPTCVariant* dp = GetDispatchParam(i);
    dp->type = type;
    NS_ABORT_IF_FALSE(!paramInfo.IsShared(), "[shared] implies [noscript]!");

    // Handle dipper types separately.
    if (paramInfo.IsDipper())
        return HandleDipperParam(dp, paramInfo);

    // Specify the correct storage/calling semantics.
    if (paramInfo.IsIndirect())
        dp->SetIndirect();

    // The JSVal proper is always stored within the 'val' union and passed
    // indirectly, regardless of in/out-ness.
    if (type_tag == nsXPTType::T_JSVAL) {
        // Root the value.
        dp->val.j = JSVAL_VOID;
        if (!JS_AddValueRoot(mCallContext, &dp->val.j))
            return false;
    }

    // Flag cleanup for anything that isn't self-contained.
    if (!type.IsArithmetic())
        dp->SetValNeedsCleanup();

    // Even if there's nothing to convert, we still need to examine the
    // JSObject container for out-params. If it's null or otherwise invalid,
    // we want to know before the call, rather than after.
    //
    // This is a no-op for 'in' params.
    jsval src;
    if (!GetOutParamSource(i, &src))
        return false;

    // All that's left to do is value conversion. Bail early if we don't need
    // to do that.
    if (!paramInfo.IsIn())
        return true;

    // We're definitely some variety of 'in' now, so there's something to
    // convert. The source value for conversion depends on whether we're
    // dealing with an 'in' or an 'inout' parameter. 'inout' was handled above,
    // so all that's left is 'in'.
    if (!paramInfo.IsOut()) {
        // Handle the 'in' case.
        NS_ASSERTION(i < mArgc || paramInfo.IsOptional(),
                     "Expected either enough arguments or an optional argument");
        if (i < mArgc)
            src = mArgv[i];
        else if (type_tag == nsXPTType::T_JSVAL)
            src = JSVAL_VOID;
        else
            src = JSVAL_NULL;
    }

    nsID param_iid;
    if (type_tag == nsXPTType::T_INTERFACE &&
        NS_FAILED(mIFaceInfo->GetIIDForParamNoAlloc(mVTableIndex, &paramInfo,
                                                    &param_iid))) {
        ThrowBadParam(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO, i, mCallContext);
        return false;
    }

    unsigned err;
    if (!XPCConvert::JSData2Native(mCallContext, &dp->val, src, type,
                                   true, &param_iid, &err)) {
        ThrowBadParam(err, i, mCallContext);
        return false;
    }

    return true;
}

JSBool
CallMethodHelper::ConvertDependentParams()
{
    const uint8_t paramCount = mMethodInfo->GetParamCount();
    for (uint8_t i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);

        if (!paramInfo.GetType().IsDependent())
            continue;
        if (!ConvertDependentParam(i))
            return false;
    }

    return true;
}

JSBool
CallMethodHelper::ConvertDependentParam(uint8_t i)
{
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
    const nsXPTType& type = paramInfo.GetType();
    nsXPTType datum_type;
    uint32_t array_count = 0;
    bool isArray = type.IsArray();

    bool isSizedString = isArray ?
        false :
        type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
        type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;

    nsXPTCVariant* dp = GetDispatchParam(i);
    dp->type = type;

    if (isArray) {
        if (NS_FAILED(mIFaceInfo->GetTypeForParam(mVTableIndex, &paramInfo, 1,
                                                  &datum_type))) {
            Throw(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, mCallContext);
            return false;
        }
        NS_ABORT_IF_FALSE(datum_type.TagPart() != nsXPTType::T_JSVAL,
                          "Arrays of JSVals not currently supported - "
                          "see bug 693337.");
    } else {
        datum_type = type;
    }

    // Specify the correct storage/calling semantics.
    if (paramInfo.IsIndirect())
        dp->SetIndirect();

    // We have 3 possible type of dependent parameters: Arrays, Sized Strings,
    // and iid_is Interface pointers. The latter two always need cleanup, and
    // arrays need cleanup for all non-arithmetic types. Since the latter two
    // cases also happen to be non-arithmetic, we can just inspect datum_type
    // here.
    if (!datum_type.IsArithmetic())
        dp->SetValNeedsCleanup();

    // Even if there's nothing to convert, we still need to examine the
    // JSObject container for out-params. If it's null or otherwise invalid,
    // we want to know before the call, rather than after.
    //
    // This is a no-op for 'in' params.
    jsval src;
    if (!GetOutParamSource(i, &src))
        return false;

    // All that's left to do is value conversion. Bail early if we don't need
    // to do that.
    if (!paramInfo.IsIn())
        return true;

    // We're definitely some variety of 'in' now, so there's something to
    // convert. The source value for conversion depends on whether we're
    // dealing with an 'in' or an 'inout' parameter. 'inout' was handled above,
    // so all that's left is 'in'.
    if (!paramInfo.IsOut()) {
        // Handle the 'in' case.
        NS_ASSERTION(i < mArgc || paramInfo.IsOptional(),
                     "Expected either enough arguments or an optional argument");
        src = i < mArgc ? mArgv[i] : JSVAL_NULL;
    }

    nsID param_iid;
    if (datum_type.IsInterfacePointer() &&
        !GetInterfaceTypeFromParam(i, datum_type, &param_iid))
        return false;

    unsigned err;

    if (isArray || isSizedString) {
        if (!GetArraySizeFromParam(i, &array_count))
            return false;

        if (isArray) {
            if (array_count &&
                !XPCConvert::JSArray2Native(mCallContext, (void**)&dp->val, src,
                                            array_count, datum_type, &param_iid,
                                            &err)) {
                // XXX need exception scheme for arrays to indicate bad element
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        } else // if (isSizedString)
        {
            if (!XPCConvert::JSStringWithSize2Native(mCallContext,
                                                     (void*)&dp->val,
                                                     src, array_count,
                                                     datum_type, &err)) {
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        }
    } else {
        if (!XPCConvert::JSData2Native(mCallContext, &dp->val, src, type,
                                       true, &param_iid, &err)) {
            ThrowBadParam(err, i, mCallContext);
            return false;
        }
    }

    return true;
}

// Performs all necessary teardown on a parameter after method invocation.
//
// This method should only be called if the value in question was flagged
// for cleanup (ie, if dp->DoesValNeedCleanup()).
void
CallMethodHelper::CleanupParam(nsXPTCMiniVariant& param, nsXPTType& type)
{
    // We handle array elements, but not the arrays themselves.
    NS_ABORT_IF_FALSE(type.TagPart() != nsXPTType::T_ARRAY, "Can't handle arrays.");

    // Pointers may sometimes be null even if cleanup was requested. Combine
    // the null checking for all the different types into one check here.
    if (type.TagPart() != nsXPTType::T_JSVAL && param.val.p == nsnull)
        return;

    switch (type.TagPart()) {
        case nsXPTType::T_JSVAL:
            JS_RemoveValueRoot(mCallContext, (jsval*)&param.val);
            break;
        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            ((nsISupports*)param.val.p)->Release();
            break;
        case nsXPTType::T_ASTRING:
        case nsXPTType::T_DOMSTRING:
            mCallContext.DeleteString((nsAString*)param.val.p);
            break;
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
            delete (nsCString*) param.val.p;
            break;
        default:
            NS_ABORT_IF_FALSE(!type.IsArithmetic(),
                              "Cleanup requested on unexpected type.");
            nsMemory::Free(param.val.p);
            break;
    }
}

// Handle parameters with dipper types.
//
// Dipper types are one of the more inscrutable aspects of xpidl. In a
// nutshell, dippers are empty container objects, created and passed by
// the caller, and filled by the callee. The callee receives a
// fully-formed object, and thus does not have to construct anything. But
// the object is functionally empty, and the callee is responsible for
// putting something useful inside of it.
//
// XPIDL decides which types to make dippers. The list of these types
// is given in the isDipperType() function in typelib.py, and is currently
// limited to 4 string types.
//
// When a dipper type is declared as an 'out' parameter, xpidl internally
// converts it to an 'in', and sets the XPT_PD_DIPPER flag on it. For this
// reason, dipper types are sometimes referred to as 'out parameters
// masquerading as in'. The burden of maintaining this illusion falls mostly
// on XPConnect - we create the empty containers, and harvest the results
// after the call.
//
// This method creates these empty containers.
JSBool
CallMethodHelper::HandleDipperParam(nsXPTCVariant* dp,
                                    const nsXPTParamInfo& paramInfo)
{
    // Get something we can make comparisons with.
    uint8_t type_tag = paramInfo.GetType().TagPart();

    // Dippers always have the 'in' and 'dipper' flags set. Never 'out'.
    NS_ABORT_IF_FALSE(!paramInfo.IsOut(), "Dipper has unexpected flags.");

    // xpidl.h specifies that dipper types will be used in exactly four
    // cases, all strings. Verify that here.
    NS_ABORT_IF_FALSE(type_tag == nsXPTType::T_ASTRING ||
                      type_tag == nsXPTType::T_DOMSTRING ||
                      type_tag == nsXPTType::T_UTF8STRING ||
                      type_tag == nsXPTType::T_CSTRING,
                      "Unexpected dipper type!");

    // ASTRING and DOMSTRING are very similar, and both use nsAutoString.
    // UTF8_STRING and CSTRING are also quite similar, and both use nsCString.
    if (type_tag == nsXPTType::T_ASTRING || type_tag == nsXPTType::T_DOMSTRING)
        dp->val.p = new nsAutoString();
    else
        dp->val.p = new nsCString();

    // Check for OOM, in either case.
    if (!dp->val.p) {
        JS_ReportOutOfMemory(mCallContext);
        return false;
    }

    // We allocated, so we need to deallocate after the method call completes.
    dp->SetValNeedsCleanup();

    return true;
}

nsresult
CallMethodHelper::Invoke()
{
    PRUint32 argc = mDispatchParams.Length();
    nsXPTCVariant* argv = mDispatchParams.Elements();

    if (XPCPerThreadData::IsMainThread(mCallContext))
        return NS_InvokeByIndex(mCallee, mVTableIndex, argc, argv);

    JSAutoSuspendRequest suspended(mCallContext);
    return NS_InvokeByIndex(mCallee, mVTableIndex, argc, argv);
}

/***************************************************************************/
// interface methods

/* readonly attribute JSObjectPtr JSObject; */
NS_IMETHODIMP XPCWrappedNative::GetJSObject(JSObject * *aJSObject)
{
    *aJSObject = GetFlatJSObject();
    return NS_OK;
}

/* readonly attribute nsISupports Native; */
NS_IMETHODIMP XPCWrappedNative::GetNative(nsISupports * *aNative)
{
    // No need to QI here, we already have the correct nsISupports
    // vtable.
    *aNative = mIdentity;
    NS_ADDREF(*aNative);
    return NS_OK;
}

/* reaonly attribute JSObjectPtr JSObjectPrototype; */
NS_IMETHODIMP XPCWrappedNative::GetJSObjectPrototype(JSObject * *aJSObjectPrototype)
{
    *aJSObjectPrototype = HasProto() ?
                GetProto()->GetJSProtoObject() : GetFlatJSObject();
    return NS_OK;
}

nsIPrincipal*
XPCWrappedNative::GetObjectPrincipal() const
{
    nsIPrincipal* principal = GetScope()->GetPrincipal();
#ifdef DEBUG
    // Because of inner window reuse, we can have objects with one principal
    // living in a scope with a different (but same-origin) principal. So
    // just check same-origin here.
    nsCOMPtr<nsIScriptObjectPrincipal> objPrin(do_QueryInterface(mIdentity));
    if (objPrin) {
        bool equal;
        if (!principal)
            equal = !objPrin->GetPrincipal();
        else
            principal->Equals(objPrin->GetPrincipal(), &equal);
        NS_ASSERTION(equal, "Principal mismatch.  Expect bad things to happen");
    }
#endif
    return principal;
}

/* readonly attribute nsIXPConnect XPConnect; */
NS_IMETHODIMP XPCWrappedNative::GetXPConnect(nsIXPConnect * *aXPConnect)
{
    if (IsValid()) {
        nsIXPConnect* temp = GetRuntime()->GetXPConnect();
        NS_IF_ADDREF(temp);
        *aXPConnect = temp;
    } else
        *aXPConnect = nsnull;
    return NS_OK;
}

/* XPCNativeInterface FindInterfaceWithMember (in jsval name); */
NS_IMETHODIMP XPCWrappedNative::FindInterfaceWithMember(jsid name, nsIInterfaceInfo * *_retval)
{
    XPCNativeInterface* iface;
    XPCNativeMember*  member;

    if (GetSet()->FindMember(name, &member, &iface) && iface) {
        nsIInterfaceInfo* temp = iface->GetInterfaceInfo();
        NS_IF_ADDREF(temp);
        *_retval = temp;
    } else
        *_retval = nsnull;
    return NS_OK;
}

/* XPCNativeInterface FindInterfaceWithName (in jsval name); */
NS_IMETHODIMP XPCWrappedNative::FindInterfaceWithName(jsid name, nsIInterfaceInfo * *_retval)
{
    XPCNativeInterface* iface = GetSet()->FindNamedInterface(name);
    if (iface) {
        nsIInterfaceInfo* temp = iface->GetInterfaceInfo();
        NS_IF_ADDREF(temp);
        *_retval = temp;
    } else
        *_retval = nsnull;
    return NS_OK;
}

inline nsresult UnexpectedFailure(nsresult rv)
{
    NS_ERROR("This is not supposed to fail!");
    return rv;
}

/* void finishInitForWrappedGlobal (); */
NS_IMETHODIMP XPCWrappedNative::FinishInitForWrappedGlobal()
{
    // We can only be called under certain conditions.
    MOZ_ASSERT(mScriptableInfo);
    MOZ_ASSERT(mScriptableInfo->GetFlags().IsGlobalObject());
    MOZ_ASSERT(HasProto());

    // Build a CCX.
    XPCCallContext ccx(NATIVE_CALLER);
    if (!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    // Call PostCreateProrotype.
    bool success = GetProto()->CallPostCreatePrototype(ccx);
    if (!success)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP XPCWrappedNative::GetSecurityInfoAddress(void*** securityInfoAddrPtr)
{
    NS_ENSURE_ARG_POINTER(securityInfoAddrPtr);
    *securityInfoAddrPtr = GetSecurityInfoAddr();
    return NS_OK;
}

/* void debugDump (in short depth); */
NS_IMETHODIMP XPCWrappedNative::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNative @ %x with mRefCnt = %d", this, mRefCnt.get()));
    XPC_LOG_INDENT();

        if (HasProto()) {
            XPCWrappedNativeProto* proto = GetProto();
            if (depth && proto)
                proto->DebugDump(depth);
            else
                XPC_LOG_ALWAYS(("mMaybeProto @ %x", proto));
        } else
            XPC_LOG_ALWAYS(("Scope @ %x", GetScope()));

        if (depth && mSet)
            mSet->DebugDump(depth);
        else
            XPC_LOG_ALWAYS(("mSet @ %x", mSet));

        XPC_LOG_ALWAYS(("mFlatJSObject of %x", mFlatJSObject));
        XPC_LOG_ALWAYS(("mIdentity of %x", mIdentity));
        XPC_LOG_ALWAYS(("mScriptableInfo @ %x", mScriptableInfo));

        if (depth && mScriptableInfo) {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mScriptable @ %x", mScriptableInfo->GetCallback()));
            XPC_LOG_ALWAYS(("mFlags of %x", (PRUint32)mScriptableInfo->GetFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %x", mScriptableInfo->GetJSClass()));
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

/***************************************************************************/

char*
XPCWrappedNative::ToString(XPCCallContext& ccx,
                           XPCWrappedNativeTearOff* to /* = nsnull */ ) const
{
#ifdef DEBUG
#  define FMT_ADDR " @ 0x%p"
#  define FMT_STR(str) str
#  define PARAM_ADDR(w) , w
#else
#  define FMT_ADDR ""
#  define FMT_STR(str)
#  define PARAM_ADDR(w)
#endif

    char* sz = nsnull;
    char* name = nsnull;

    XPCNativeScriptableInfo* si = GetScriptableInfo();
    if (si)
        name = JS_smprintf("%s", si->GetJSClass()->name);
    if (to) {
        const char* fmt = name ? " (%s)" : "%s";
        name = JS_sprintf_append(name, fmt,
                                 to->GetInterface()->GetNameString());
    } else if (!name) {
        XPCNativeSet* set = GetSet();
        XPCNativeInterface** array = set->GetInterfaceArray();
        PRUint16 count = set->GetInterfaceCount();

        if (count == 1)
            name = JS_sprintf_append(name, "%s", array[0]->GetNameString());
        else if (count == 2 &&
                 array[0] == XPCNativeInterface::GetISupports(ccx)) {
            name = JS_sprintf_append(name, "%s", array[1]->GetNameString());
        } else {
            for (PRUint16 i = 0; i < count; i++) {
                const char* fmt = (i == 0) ?
                                    "(%s" : (i == count-1) ?
                                        ", %s)" : ", %s";
                name = JS_sprintf_append(name, fmt,
                                         array[i]->GetNameString());
            }
        }
    }

    if (!name) {
        return nsnull;
    }
    const char* fmt = "[xpconnect wrapped %s" FMT_ADDR FMT_STR(" (native")
        FMT_ADDR FMT_STR(")") "]";
    if (si) {
        fmt = "[object %s" FMT_ADDR FMT_STR(" (native") FMT_ADDR FMT_STR(")") "]";
    }
    sz = JS_smprintf(fmt, name PARAM_ADDR(this) PARAM_ADDR(mIdentity));

    JS_smprintf_free(name);


    return sz;

#undef FMT_ADDR
#undef PARAM_ADDR
}

/***************************************************************************/

#ifdef XPC_CHECK_CLASSINFO_CLAIMS
static void DEBUG_CheckClassInfoClaims(XPCWrappedNative* wrapper)
{
    if (!wrapper || !wrapper->GetClassInfo())
        return;

    nsISupports* obj = wrapper->GetIdentityObject();
    XPCNativeSet* set = wrapper->GetSet();
    PRUint16 count = set->GetInterfaceCount();
    for (PRUint16 i = 0; i < count; i++) {
        nsIClassInfo* clsInfo = wrapper->GetClassInfo();
        XPCNativeInterface* iface = set->GetInterfaceAt(i);
        nsIInterfaceInfo* info = iface->GetInterfaceInfo();
        const nsIID* iid;
        nsISupports* ptr;

        info->GetIIDShared(&iid);
        nsresult rv = obj->QueryInterface(*iid, (void**)&ptr);
        if (NS_SUCCEEDED(rv)) {
            NS_RELEASE(ptr);
            continue;
        }
        if (rv == NS_ERROR_OUT_OF_MEMORY)
            continue;

        // Houston, We have a problem...

        char* className = nsnull;
        char* contractID = nsnull;
        const char* interfaceName;

        info->GetNameShared(&interfaceName);
        clsInfo->GetContractID(&contractID);
        if (wrapper->GetScriptableInfo()) {
            wrapper->GetScriptableInfo()->GetCallback()->
                GetClassName(&className);
        }


        printf("\n!!! Object's nsIClassInfo lies about its interfaces!!!\n"
               "   classname: %s \n"
               "   contractid: %s \n"
               "   unimplemented interface name: %s\n\n",
               className ? className : "<unknown>",
               contractID ? contractID : "<unknown>",
               interfaceName);

#ifdef XPC_ASSERT_CLASSINFO_CLAIMS
        NS_ERROR("Fix this QueryInterface or nsIClassInfo");
#endif

        if (className)
            nsMemory::Free(className);
        if (contractID)
            nsMemory::Free(contractID);
    }
}
#endif

#ifdef XPC_REPORT_SHADOWED_WRAPPED_NATIVE_MEMBERS
static void DEBUG_PrintShadowObjectInfo(const char* header,
                                        XPCNativeSet* set,
                                        XPCWrappedNative* wrapper,
                                        XPCWrappedNativeProto* proto)

{
    if (header)
        printf("%s\n", header);

    printf("   XPCNativeSet @ 0x%p for the class:\n", (void*)set);

    char* className = nsnull;
    char* contractID = nsnull;

    nsIClassInfo* clsInfo = proto ? proto->GetClassInfo() : nsnull;
    if (clsInfo)
        clsInfo->GetContractID(&contractID);

    XPCNativeScriptableInfo* si = wrapper ?
            wrapper->GetScriptableInfo() :
            proto->GetScriptableInfo();
    if (si)
        si->GetCallback()->GetClassName(&className);

    printf("   classname: %s \n"
           "   contractid: %s \n",
           className ? className : "<unknown>",
           contractID ? contractID : "<unknown>");

    if (className)
        nsMemory::Free(className);
    if (contractID)
        nsMemory::Free(contractID);

    printf("   claims to implement interfaces:\n");

    PRUint16 count = set->GetInterfaceCount();
    for (PRUint16 i = 0; i < count; i++) {
        XPCNativeInterface* iface = set->GetInterfaceAt(i);
        nsIInterfaceInfo* info = iface->GetInterfaceInfo();
        const char* interfaceName;
        info->GetNameShared(&interfaceName);
        printf("      %s\n", interfaceName);
    }
}

static void ReportSingleMember(jsval ifaceName,
                               jsval memberName)
{
    JS_FileEscapedString(stdout, ifaceName, 0);
    if (JSVAL_IS_STRING(memberName)) {
        fputs("::", stdout);
        JS_FileEscapedString(stdout, memberName, 0);
    }
}

static void ShowHeader(JSBool* printedHeader,
                       const char* header,
                       XPCNativeSet* set,
                       XPCWrappedNative* wrapper,
                       XPCWrappedNativeProto* proto)
{
    if (!*printedHeader) {
        DEBUG_PrintShadowObjectInfo(header, set, wrapper, proto);
        *printedHeader = true;
    }

}

static void ShowOneShadow(jsval ifaceName1,
                          jsval memberName1,
                          jsval ifaceName2,
                          jsval memberName2)
{
    ReportSingleMember(ifaceName1, memberName1);
    printf(" shadows ");
    ReportSingleMember(ifaceName2, memberName2);
    printf("\n");
}

static void ShowDuplicateInterface(jsval ifaceName)
{
    fputs(" ! ", stdout);
    JS_FileEscapedString(stdout, ifaceName, 0);
    fputs(" appears twice in the nsIClassInfo interface set!\n", stdout);
}

static JSBool InterfacesAreRelated(XPCNativeInterface* iface1,
                                   XPCNativeInterface* iface2)
{
    nsIInterfaceInfo* info1 = iface1->GetInterfaceInfo();
    nsIInterfaceInfo* info2 = iface2->GetInterfaceInfo();

    NS_ASSERTION(info1 != info2, "should not have different iface!");

    bool match;

    return
        (NS_SUCCEEDED(info1->HasAncestor(iface2->GetIID(), &match)) && match) ||
        (NS_SUCCEEDED(info2->HasAncestor(iface1->GetIID(), &match)) && match);
}

static JSBool MembersAreTheSame(XPCNativeInterface* iface1,
                                PRUint16 memberIndex1,
                                XPCNativeInterface* iface2,
                                PRUint16 memberIndex2)
{
    nsIInterfaceInfo* info1 = iface1->GetInterfaceInfo();
    nsIInterfaceInfo* info2 = iface2->GetInterfaceInfo();

    XPCNativeMember* member1 = iface1->GetMemberAt(memberIndex1);
    XPCNativeMember* member2 = iface2->GetMemberAt(memberIndex2);

    PRUint16 index1 = member1->GetIndex();
    PRUint16 index2 = member2->GetIndex();

    // If they are both constants, then we'll just be sure that they are equivalent.

    if (member1->IsConstant()) {
        if (!member2->IsConstant())
            return false;

        const nsXPTConstant* constant1;
        const nsXPTConstant* constant2;

        return NS_SUCCEEDED(info1->GetConstant(index1, &constant1)) &&
               NS_SUCCEEDED(info2->GetConstant(index2, &constant2)) &&
               constant1->GetType() == constant2->GetType() &&
               constant1->GetValue() == constant2->GetValue();
    }

    // Else we make sure they are of the same 'type' and return true only if
    // they are inherited from the same interface.

    if (member1->IsMethod() != member2->IsMethod() ||
        member1->IsWritableAttribute() != member2->IsWritableAttribute() ||
        member1->IsReadOnlyAttribute() != member2->IsReadOnlyAttribute()) {
        return false;
    }

    const nsXPTMethodInfo* mi1;
    const nsXPTMethodInfo* mi2;

    return NS_SUCCEEDED(info1->GetMethodInfo(index1, &mi1)) &&
           NS_SUCCEEDED(info2->GetMethodInfo(index2, &mi2)) &&
           mi1 == mi2;
}

void DEBUG_ReportShadowedMembers(XPCNativeSet* set,
                                 XPCWrappedNative* wrapper,
                                 XPCWrappedNativeProto* proto)
{
    // NOTE: Either wrapper or proto could be null...

    if (!(proto || wrapper) || !set || set->GetInterfaceCount() < 2)
        return;

    NS_ASSERTION(proto || wrapper, "bad param!");
    XPCJSRuntime* rt = proto ? proto->GetRuntime() : wrapper->GetRuntime();

    // a quicky hack to avoid reporting info for the same set too often
    static int nextSeenSet = 0;
    static const int MAX_SEEN_SETS = 128;
    static XPCNativeSet* SeenSets[MAX_SEEN_SETS];
    for (int seen = 0; seen < MAX_SEEN_SETS; seen++)
        if (set == SeenSets[seen])
            return;
    SeenSets[nextSeenSet] = set;

#ifdef off_DEBUG_jband
    static int seenCount = 0;
    printf("--- adding SeenSets[%d] = 0x%p\n", nextSeenSet, set);
    DEBUG_PrintShadowObjectInfo(nsnull, set, wrapper, proto);
#endif
    int localNext = nextSeenSet+1;
    nextSeenSet = localNext < MAX_SEEN_SETS ? localNext : 0;

    XPCNativeScriptableInfo* si = wrapper ?
            wrapper->GetScriptableInfo() :
            proto->GetScriptableInfo();

    // We just want to skip some classes...
    if (si) {
        // Add any classnames to skip to this (null terminated) array...
        static const char* skipClasses[] = {
            "Window",
            "HTMLDocument",
            "HTMLCollection",
            "Event",
            "ChromeWindow",
            nsnull
        };

        static bool warned = false;
        if (!warned) {
            printf("!!! XPConnect won't warn about Shadowed Members of...\n  ");
            for (const char** name = skipClasses; *name; name++)
                printf("%s %s", name == skipClasses ? "" : ",", *name);
             printf("\n");
            warned = true;
        }

        bool quit = false;
        char* className = nsnull;
        si->GetCallback()->GetClassName(&className);
        if (className) {
            for (const char** name = skipClasses; *name; name++) {
                if (!strcmp(*name, className)) {
                    quit = true;
                    break;
                }
            }
            nsMemory::Free(className);
        }
        if (quit)
            return;
    }

    const char header[] =
        "!!!Object wrapped by XPConnect has members whose names shadow each other!!!";

    JSBool printedHeader = false;

    jsval QIName = rt->GetStringJSVal(XPCJSRuntime::IDX_QUERY_INTERFACE);

    PRUint16 ifaceCount = set->GetInterfaceCount();
    PRUint16 i, j, k, m;

    // First look for duplicate interface entries

    for (i = 0; i < ifaceCount; i++) {
        XPCNativeInterface* ifaceOuter = set->GetInterfaceAt(i);
        for (k = i+1; k < ifaceCount; k++) {
            XPCNativeInterface* ifaceInner = set->GetInterfaceAt(k);
            if (ifaceInner == ifaceOuter) {
                ShowHeader(&printedHeader, header, set, wrapper, proto);
                ShowDuplicateInterface(ifaceOuter->GetName());
            }
        }
    }

    // Now scan for shadowing names

    for (i = 0; i < ifaceCount; i++) {
        XPCNativeInterface* ifaceOuter = set->GetInterfaceAt(i);
        jsval ifaceOuterName = ifaceOuter->GetName();

        PRUint16 memberCountOuter = ifaceOuter->GetMemberCount();
        for (j = 0; j < memberCountOuter; j++) {
            XPCNativeMember* memberOuter = ifaceOuter->GetMemberAt(j);
            jsval memberOuterName = memberOuter->GetName();

            if (memberOuterName == QIName)
                continue;

            for (k = i+1; k < ifaceCount; k++) {
                XPCNativeInterface* ifaceInner = set->GetInterfaceAt(k);
                jsval ifaceInnerName = ifaceInner->GetName();

                // Reported elsewhere.
                if (ifaceInner == ifaceOuter)
                    continue;

                // We consider this not worth reporting because callers will
                // almost certainly be getting what they expect.
                if (InterfacesAreRelated(ifaceInner, ifaceOuter))
                    continue;

                if (ifaceInnerName == memberOuterName) {
                    ShowHeader(&printedHeader, header, set, wrapper, proto);
                    ShowOneShadow(ifaceInnerName, JSVAL_NULL,
                                  ifaceOuterName, memberOuterName);
                }

                PRUint16 memberCountInner = ifaceInner->GetMemberCount();

                for (m = 0; m < memberCountInner; m++) {
                    XPCNativeMember* memberInner = ifaceInner->GetMemberAt(m);
                    jsval memberInnerName = memberInner->GetName();

                    if (memberInnerName == QIName)
                        continue;

                    if (memberOuterName == memberInnerName &&
                        !MembersAreTheSame(ifaceOuter, j, ifaceInner, m))

                    {
                        ShowHeader(&printedHeader, header, set, wrapper, proto);
                        ShowOneShadow(ifaceOuterName, memberOuterName,
                                      ifaceInnerName, memberInnerName);
                    }
                }
            }
        }
    }
}
#endif

#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
void DEBUG_ReportWrapperThreadSafetyError(XPCCallContext& ccx,
                                          const char* msg,
                                          const XPCWrappedNative* wrapper)
{
    XPCPerThreadData* tls = ccx.GetThreadData();
    if (1 != tls->IncrementWrappedNativeThreadsafetyReportDepth())
        return;

    printf("---------------------------------------------------------------\n");
    printf("!!!!! XPConnect wrapper thread use error...\n");

    char* wrapperDump = wrapper->ToString(ccx);
    if (wrapperDump) {
        printf("  %s\n  wrapper: %s\n", msg, wrapperDump);
        JS_smprintf_free(wrapperDump);
    } else
        printf("  %s\n  wrapper @ 0x%p\n", msg, (void *)wrapper);

    printf("  JS call stack...\n");
    xpc_DumpJSStack(ccx, true, true, true);
    printf("---------------------------------------------------------------\n");

    tls->ClearWrappedNativeThreadsafetyReportDepth();
}

void DEBUG_CheckWrapperThreadSafety(const XPCWrappedNative* wrapper)
{
    XPCWrappedNativeProto* proto = wrapper->GetProto();
    if (proto && proto->ClassIsThreadSafe())
        return;

    if (proto && proto->ClassIsMainThreadOnly()) {
        // NS_IsMainThread is safe to call even after we've started shutting
        // down.
        if (!NS_IsMainThread()) {
            XPCCallContext ccx(NATIVE_CALLER);
            DEBUG_ReportWrapperThreadSafetyError(ccx,
                                                 "Main Thread Only wrapper accessed on another thread", wrapper);
        }
    } else if (PR_GetCurrentThread() != wrapper->mThread) {
        XPCCallContext ccx(NATIVE_CALLER);
        DEBUG_ReportWrapperThreadSafetyError(ccx,
                                             "XPConnect WrappedNative is being accessed on multiple threads but "
                                             "the underlying native xpcom object does not have a "
                                             "nsIClassInfo with the 'THREADSAFE' flag set", wrapper);
    }
}
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(XPCJSObjectHolder, nsIXPConnectJSObjectHolder)

NS_IMETHODIMP
XPCJSObjectHolder::GetJSObject(JSObject** aJSObj)
{
    NS_PRECONDITION(aJSObj, "bad param");
    NS_PRECONDITION(mJSObj, "bad object state");
    *aJSObj = mJSObj;
    return NS_OK;
}

XPCJSObjectHolder::XPCJSObjectHolder(XPCCallContext& ccx, JSObject* obj)
    : mJSObj(obj)
{
    ccx.GetRuntime()->AddObjectHolderRoot(this);
}

XPCJSObjectHolder::~XPCJSObjectHolder()
{
    RemoveFromRootSet(nsXPConnect::GetRuntimeInstance()->GetMapLock());
}

void
XPCJSObjectHolder::TraceJS(JSTracer *trc)
{
    JS_SET_TRACING_DETAILS(trc, GetTraceName, this, 0);
    JS_CallTracer(trc, mJSObj, JSTRACE_OBJECT);
}

// static
void
XPCJSObjectHolder::GetTraceName(JSTracer* trc, char *buf, size_t bufsize)
{
    JS_snprintf(buf, bufsize, "XPCJSObjectHolder[0x%p].mJSObj",
                trc->debugPrintArg);
}

// static
XPCJSObjectHolder*
XPCJSObjectHolder::newHolder(XPCCallContext& ccx, JSObject* obj)
{
    if (!obj) {
        NS_ERROR("bad param");
        return nsnull;
    }
    return new XPCJSObjectHolder(ccx, obj);
}

JSBool
MorphSlimWrapper(JSContext *cx, JSObject *obj)
{
    SLIM_LOG(("***** morphing from MorphSlimToWrapper (%p, %p)\n",
              obj, static_cast<nsISupports*>(xpc_GetJSPrivate(obj))));

    XPCCallContext ccx(JS_CALLER, cx);

    nsISupports* object = static_cast<nsISupports*>(xpc_GetJSPrivate(obj));
    nsWrapperCache *cache = nsnull;
    CallQueryInterface(object, &cache);
    nsRefPtr<XPCWrappedNative> wn;
    nsresult rv = XPCWrappedNative::Morph(ccx, obj, nsnull, cache,
                                          getter_AddRefs(wn));
    return NS_SUCCEEDED(rv);
}

#ifdef DEBUG_slimwrappers
static PRUint32 sSlimWrappers;
#endif

JSBool
ConstructSlimWrapper(XPCCallContext &ccx,
                     xpcObjectHelper &aHelper,
                     XPCWrappedNativeScope* xpcScope, jsval *rval)
{
    nsISupports *identityObj = aHelper.GetCanonical();
    nsXPCClassInfo *classInfoHelper = aHelper.GetXPCClassInfo();

    uint32_t flagsInt;
    nsresult rv = classInfoHelper->GetScriptableFlags(&flagsInt);
    if (NS_FAILED(rv))
        flagsInt = 0;

    XPCNativeScriptableFlags flags(flagsInt);

    NS_ASSERTION(flags.DontAskInstanceForScriptable(),
                 "Not supported for cached wrappers!");

    JSObject* parent = xpcScope->GetGlobalJSObject();
    if (!flags.WantPreCreate()) {
        SLIM_LOG_NOT_CREATED(ccx, identityObj,
                             "scriptable helper has no PreCreate hook");

        return false;
    }

    JSObject* plannedParent = parent;
    rv = classInfoHelper->PreCreate(identityObj, ccx, parent, &parent);
    if (rv != NS_SUCCESS_ALLOW_SLIM_WRAPPERS) {
        SLIM_LOG_NOT_CREATED(ccx, identityObj, "PreCreate hook refused");

        return false;
    }

    if (!js::IsObjectInContextCompartment(parent, ccx.GetJSContext())) {
        SLIM_LOG_NOT_CREATED(ccx, identityObj, "wrong compartment");

        return false;
    }

    JSAutoEnterCompartment ac;
    if (!ac.enter(ccx, parent)) {
        SLIM_LOG_NOT_CREATED(ccx, identityObj, "unable to enter compartment");

        return false;
    }

    if (parent != plannedParent) {
        XPCWrappedNativeScope *newXpcScope =
            XPCWrappedNativeScope::FindInJSObjectScope(ccx, parent);
        if (newXpcScope != xpcScope) {
            SLIM_LOG_NOT_CREATED(ccx, identityObj, "crossing origins");

            return false;
        }
    }

    // The PreCreate hook could have forced the creation of a wrapper, need
    // to check for that here and return early.
    nsWrapperCache *cache = aHelper.GetWrapperCache();
    JSObject* wrapper = cache->GetWrapper();
    if (wrapper) {
        *rval = OBJECT_TO_JSVAL(wrapper);

        return true;
    }

    PRUint32 interfacesBitmap = classInfoHelper->GetInterfacesBitmap();
    XPCNativeScriptableCreateInfo
        sciProto(aHelper.forgetXPCClassInfo(), flags, interfacesBitmap);

    AutoMarkingWrappedNativeProtoPtr xpcproto(ccx);
    xpcproto = XPCWrappedNativeProto::GetNewOrUsed(ccx, xpcScope,
                                                   classInfoHelper, &sciProto);
    if (!xpcproto)
        return false;

    xpcproto->CacheOffsets(identityObj);

    XPCNativeScriptableInfo* si = xpcproto->GetScriptableInfo();
    JSClass* jsclazz = si->GetSlimJSClass();
    if (!jsclazz)
        return false;

    wrapper = xpc_NewSystemInheritingJSObject(ccx, jsclazz,
                                              xpcproto->GetJSProtoObject(),
                                              false, parent);
    if (!wrapper)
        return false;

    JS_SetPrivate(wrapper, identityObj);
    JS_SetReservedSlot(wrapper, 0, PRIVATE_TO_JSVAL(xpcproto.get()));

    // Transfer ownership to the wrapper's private.
    aHelper.forgetCanonical();

    cache->SetWrapper(wrapper);

    SLIM_LOG(("+++++ %i created slim wrapper (%p, %p, %p)\n", ++sSlimWrappers,
              wrapper, p, xpcScope));

    *rval = OBJECT_TO_JSVAL(wrapper);

    return true;
}
