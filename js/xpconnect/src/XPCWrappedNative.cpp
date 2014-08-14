/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Wrapper object for reflecting native xpcom objects into JavaScript. */

#include "xpcprivate.h"
#include "nsWrapperCacheInlines.h"
#include "XPCLog.h"
#include "jsprf.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "XrayWrapper.h"

#include "nsContentUtils.h"
#include "nsCxPusher.h"

#include <stdint.h>
#include "mozilla/Likely.h"
#include "mozilla/dom/BindingUtils.h"
#include <algorithm>

using namespace xpc;
using namespace mozilla;
using namespace mozilla::dom;
using namespace JS;

/***************************************************************************/

NS_IMPL_CYCLE_COLLECTION_CLASS(XPCWrappedNative)

// No need to unlink the JS objects: if the XPCWrappedNative is cycle
// collected then its mFlatJSObject will be cycle collected too and
// finalization of the mFlatJSObject will unlink the JS objects (see
// XPC_WN_NoHelper_Finalize and FlatJSObjectFinalized).
NS_IMETHODIMP_(void)
NS_CYCLE_COLLECTION_CLASSNAME(XPCWrappedNative)::Unlink(void *p)
{
    XPCWrappedNative *tmp = static_cast<XPCWrappedNative*>(p);
    tmp->ExpireWrapper();
}

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(XPCWrappedNative)::Traverse
   (void *p, nsCycleCollectionTraversalCallback &cb)
{
    XPCWrappedNative *tmp = static_cast<XPCWrappedNative*>(p);
    if (!tmp->IsValid())
        return NS_OK;

    if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
        char name[72];
        XPCNativeScriptableInfo* si = tmp->GetScriptableInfo();
        if (si)
            JS_snprintf(name, sizeof(name), "XPCWrappedNative (%s)",
                        si->GetJSClass()->name);
        else
            JS_snprintf(name, sizeof(name), "XPCWrappedNative");

        cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
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

/***************************************************************************/
static nsresult
FinishCreate(XPCWrappedNativeScope* Scope,
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
XPCWrappedNative::WrapNewGlobal(xpcObjectHelper &nativeHelper,
                                nsIPrincipal *principal,
                                bool initStandardClasses,
                                JS::CompartmentOptions& aOptions,
                                XPCWrappedNative **wrappedGlobal)
{
    AutoJSContext cx;
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
    AutoMarkingNativeScriptableInfoPtr si(cx, XPCNativeScriptableInfo::Construct(&sciWrapper));
    MOZ_ASSERT(si.get());

    // Finally, we get to the JSClass.
    const JSClass *clasp = si->GetJSClass();
    MOZ_ASSERT(clasp->flags & JSCLASS_IS_GLOBAL);

    // Create the global.
    aOptions.setTrace(XPCWrappedNative::Trace);
    RootedObject global(cx, xpc::CreateGlobalObject(cx, clasp, principal, aOptions));
    if (!global)
        return NS_ERROR_FAILURE;
    XPCWrappedNativeScope *scope = CompartmentPrivate::Get(global)->scope;

    // Immediately enter the global's compartment, so that everything else we
    // create ends up there.
    JSAutoCompartment ac(cx, global);

    // If requested, initialize the standard classes on the global.
    if (initStandardClasses && ! JS_InitStandardClasses(cx, global))
        return NS_ERROR_FAILURE;

    // Make a proto.
    XPCWrappedNativeProto *proto =
        XPCWrappedNativeProto::GetNewOrUsed(scope,
                                            nativeHelper.GetClassInfo(), &sciProto,
                                            /* callPostCreatePrototype = */ false);
    if (!proto)
        return NS_ERROR_FAILURE;

    // Set up the prototype on the global.
    MOZ_ASSERT(proto->GetJSProtoObject());
    RootedObject protoObj(cx, proto->GetJSProtoObject());
    bool success = JS_SplicePrototype(cx, global, protoObj);
    if (!success)
        return NS_ERROR_FAILURE;

    // Construct the wrapper, which takes over the strong reference to the
    // native object.
    nsRefPtr<XPCWrappedNative> wrapper =
        new XPCWrappedNative(nativeHelper.forgetCanonical(), proto);

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
        // XPCNativeScriptableShared instances live in a map, and are
        // GCed, but XPCNativeScriptableInfo is per-instance and must be
        // manually managed. If we're switching over to that of the proto, we
        // need to destroy the one we've allocated, and also null out the
        // AutoMarkingPtr, so that it doesn't try to mark garbage data.
        delete si;
        si = nullptr;
    } else {
        wrapper->mScriptableInfo = si;
    }

    // Set the JS object to the global we already created.
    wrapper->mFlatJSObject = global;
    wrapper->mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

    // Set the private to the XPCWrappedNative.
    JS_SetPrivate(global, wrapper);

    // There are dire comments elsewhere in the code about how a GC can
    // happen somewhere after wrapper initialization but before the wrapper is
    // added to the hashtable in FinishCreate(). It's not clear if that can
    // happen here, but let's just be safe for now.
    AutoMarkingWrappedNativePtr wrapperMarker(cx, wrapper);

    // Call the common Init finish routine. This mainly just does an AddRef
    // on behalf of XPConnect (the corresponding Release is in the finalizer
    // hook), but it does some other miscellaneous things too, so we don't
    // inline it.
    success = wrapper->FinishInit();
    MOZ_ASSERT(success);

    // Go through some extra work to find the tearoff. This is kind of silly
    // on a conceptual level: the point of tearoffs is to cache the results
    // of QI-ing mIdentity to different interfaces, and we don't need that
    // since we're dealing with nsISupports. But lots of code expects tearoffs
    // to exist for everything, so we just follow along.
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(&NS_GET_IID(nsISupports));
    MOZ_ASSERT(iface);
    nsresult status;
    success = wrapper->FindTearOff(iface, false, &status);
    if (!success)
        return status;

    // Call the common creation finish routine. This does all of the bookkeeping
    // like inserting the wrapper into the wrapper map and setting up the wrapper
    // cache.
    nsresult rv = FinishCreate(scope, iface, nativeHelper.GetWrapperCache(),
                               wrapper, wrappedGlobal);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

// static
nsresult
XPCWrappedNative::GetNewOrUsed(xpcObjectHelper& helper,
                               XPCWrappedNativeScope* Scope,
                               XPCNativeInterface* Interface,
                               XPCWrappedNative** resultWrapper)
{
    MOZ_ASSERT(Interface);
    AutoJSContext cx;
    nsWrapperCache *cache = helper.GetWrapperCache();

    MOZ_ASSERT(!cache || !cache->GetWrapperPreserveColor(),
               "We assume the caller already checked if it could get the "
               "wrapper from the cache.");

    nsresult rv;

    MOZ_ASSERT(!Scope->GetRuntime()->GCIsRunning(),
               "XPCWrappedNative::GetNewOrUsed called during GC");

    nsISupports *identity = helper.GetCanonical();

    if (!identity) {
        NS_ERROR("This XPCOM object fails in QueryInterface to nsISupports!");
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<XPCWrappedNative> wrapper;

    Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();
    // Some things are nsWrapperCache subclasses but never use the cache, so go
    // ahead and check our map even if we have a cache and it has no existing
    // wrapper: we might have an XPCWrappedNative anyway.
    wrapper = map->Find(identity);

    if (wrapper) {
        if (!wrapper->FindTearOff(Interface, false, &rv)) {
            MOZ_ASSERT(NS_FAILED(rv), "returning NS_OK on failure");
            return rv;
        }
        wrapper.forget(resultWrapper);
        return NS_OK;
    }

    // There is a chance that the object wants to have the self-same JSObject
    // reflection regardless of the scope into which we are reflecting it.
    // Many DOM objects require this. The scriptable helper specifies this
    // in preCreate by indicating a 'parent' of a particular scope.
    //
    // To handle this we need to get the scriptable helper early and ask it.
    // It is possible that we will then end up forwarding this entire call
    // to this same function but with a different scope.

    // If we are making a wrapper for an nsIClassInfo singleton then
    // We *don't* want to have it use the prototype meant for instances
    // of that class.
    uint32_t classInfoFlags;
    bool isClassInfoSingleton = helper.GetClassInfo() == helper.Object() &&
                                NS_SUCCEEDED(helper.GetClassInfo()
                                                   ->GetFlags(&classInfoFlags)) &&
                                (classInfoFlags & nsIClassInfo::SINGLETON_CLASSINFO);

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
        isClassInfoSingleton ? sci :
        GatherScriptableCreateInfo(identity, info, sciProto, sci);

    RootedObject parent(cx, Scope->GetGlobalJSObject());

    RootedValue newParentVal(cx, NullValue());

    mozilla::Maybe<JSAutoCompartment> ac;

    if (sciWrapper.GetFlags().WantPreCreate()) {
        RootedObject plannedParent(cx, parent);
        nsresult rv = sciWrapper.GetCallback()->PreCreate(identity, cx,
                                                          parent, parent.address());
        if (NS_FAILED(rv))
            return rv;
        rv = NS_OK;

        MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(parent),
                   "Xray wrapper being used to parent XPCWrappedNative?");

        ac.emplace(static_cast<JSContext*>(cx), parent);

        if (parent != plannedParent) {
            XPCWrappedNativeScope* betterScope = ObjectScope(parent);
            if (betterScope != Scope)
                return GetNewOrUsed(helper, betterScope, Interface, resultWrapper);

            newParentVal = OBJECT_TO_JSVAL(parent);
        }

        // Take the performance hit of checking the hashtable again in case
        // the preCreate call caused the wrapper to get created through some
        // interesting path (the DOM code tends to make this happen sometimes).

        if (cache) {
            RootedObject cached(cx, cache->GetWrapper());
            if (cached)
                wrapper = XPCWrappedNative::Get(cached);
        } else {
            wrapper = map->Find(identity);
        }

        if (wrapper) {
            if (wrapper->FindTearOff(Interface, false, &rv)) {
                MOZ_ASSERT(NS_FAILED(rv), "returning NS_OK on failure");
                return rv;
            }
            wrapper.forget(resultWrapper);
            return NS_OK;
        }
    } else {
        ac.emplace(static_cast<JSContext*>(cx), parent);
    }

    AutoMarkingWrappedNativeProtoPtr proto(cx);

    // If there is ClassInfo (and we are not building a wrapper for the
    // nsIClassInfo interface) then we use a wrapper that needs a prototype.

    // Note that the security check happens inside FindTearOff - after the
    // wrapper is actually created, but before JS code can see it.

    if (info && !isClassInfoSingleton) {
        proto = XPCWrappedNativeProto::GetNewOrUsed(Scope, info, &sciProto);
        if (!proto)
            return NS_ERROR_FAILURE;

        wrapper = new XPCWrappedNative(helper.forgetCanonical(), proto);
    } else {
        AutoMarkingNativeInterfacePtr iface(cx, Interface);
        if (!iface)
            iface = XPCNativeInterface::GetISupports();

        AutoMarkingNativeSetPtr set(cx);
        set = XPCNativeSet::GetNewOrUsed(nullptr, iface, 0);

        if (!set)
            return NS_ERROR_FAILURE;

        wrapper =
            new XPCWrappedNative(helper.forgetCanonical(), Scope, set);
    }

    MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(parent),
               "Xray wrapper being used to parent XPCWrappedNative?");

    // We use an AutoMarkingPtr here because it is possible for JS gc to happen
    // after we have Init'd the wrapper but *before* we add it to the hashtable.
    // This would cause the mSet to get collected and we'd later crash. I've
    // *seen* this happen.
    AutoMarkingWrappedNativePtr wrapperMarker(cx, wrapper);

    if (!wrapper->Init(parent, &sciWrapper))
        return NS_ERROR_FAILURE;

    if (!wrapper->FindTearOff(Interface, false, &rv)) {
        MOZ_ASSERT(NS_FAILED(rv), "returning NS_OK on failure");
        return rv;
    }

    return FinishCreate(Scope, Interface, cache, wrapper, resultWrapper);
}

static nsresult
FinishCreate(XPCWrappedNativeScope* Scope,
             XPCNativeInterface* Interface,
             nsWrapperCache *cache,
             XPCWrappedNative* inWrapper,
             XPCWrappedNative** resultWrapper)
{
    AutoJSContext cx;
    MOZ_ASSERT(inWrapper);

    Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();

    nsRefPtr<XPCWrappedNative> wrapper;
    // Deal with the case where the wrapper got created as a side effect
    // of one of our calls out of this code. Add() returns the (possibly
    // pre-existing) wrapper that ultimately ends up in the map, which is
    // what we want.
    wrapper = map->Add(inWrapper);
    if (!wrapper)
        return NS_ERROR_FAILURE;

    if (wrapper == inWrapper) {
        JSObject *flat = wrapper->GetFlatJSObject();
        MOZ_ASSERT(!cache || !cache->GetWrapperPreserveColor() ||
                   flat == cache->GetWrapperPreserveColor(),
                   "This object has a cached wrapper that's different from "
                   "the JSObject held by its native wrapper?");

        if (cache && !cache->GetWrapperPreserveColor())
            cache->SetWrapper(flat);

        // Our newly created wrapper is the one that we just added to the table.
        // All is well. Call PostCreate as necessary.
        XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
        if (si && si->GetFlags().WantPostCreate()) {
            nsresult rv = si->GetCallback()->PostCreate(wrapper, cx, flat);
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

                map->Remove(wrapper);

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
    wrapper.forget(resultWrapper);
    return NS_OK;
}

// static
nsresult
XPCWrappedNative::GetUsedOnly(nsISupports* Object,
                              XPCWrappedNativeScope* Scope,
                              XPCNativeInterface* Interface,
                              XPCWrappedNative** resultWrapper)
{
    AutoJSContext cx;
    MOZ_ASSERT(Object, "XPCWrappedNative::GetUsedOnly was called with a null Object");
    MOZ_ASSERT(Interface);

    nsRefPtr<XPCWrappedNative> wrapper;
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(Object, &cache);
    if (cache) {
        RootedObject flat(cx, cache->GetWrapper());
        if (!flat) {
            *resultWrapper = nullptr;
            return NS_OK;
        }
        wrapper = XPCWrappedNative::Get(flat);
    } else {
        nsCOMPtr<nsISupports> identity = do_QueryInterface(Object);

        if (!identity) {
            NS_ERROR("This XPCOM object fails in QueryInterface to nsISupports!");
            return NS_ERROR_FAILURE;
        }

        Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();

        wrapper = map->Find(identity);
        if (!wrapper) {
            *resultWrapper = nullptr;
            return NS_OK;
        }
    }

    nsresult rv;
    if (!wrapper->FindTearOff(Interface, false, &rv)) {
        MOZ_ASSERT(NS_FAILED(rv), "returning NS_OK on failure");
        return rv;
    }

    wrapper.forget(resultWrapper);
    return NS_OK;
}

// This ctor is used if this object will have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                                   XPCWrappedNativeProto* aProto)
    : mMaybeProto(aProto),
      mSet(aProto->GetSet()),
      mScriptableInfo(nullptr)
{
    MOZ_ASSERT(NS_IsMainThread());

    mIdentity = aIdentity.take();
    mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

    MOZ_ASSERT(mMaybeProto, "bad ctor param");
    MOZ_ASSERT(mSet, "bad ctor param");
}

// This ctor is used if this object will NOT have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                                   XPCWrappedNativeScope* aScope,
                                   XPCNativeSet* aSet)

    : mMaybeScope(TagScope(aScope)),
      mSet(aSet),
      mScriptableInfo(nullptr)
{
    MOZ_ASSERT(NS_IsMainThread());

    mIdentity = aIdentity.take();
    mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

    MOZ_ASSERT(aScope, "bad ctor param");
    MOZ_ASSERT(aSet, "bad ctor param");
}

XPCWrappedNative::~XPCWrappedNative()
{
    Destroy();
}

void
XPCWrappedNative::Destroy()
{
    XPCWrappedNativeProto* proto = GetProto();

    if (mScriptableInfo &&
        (!HasProto() ||
         (proto && proto->GetScriptableInfo() != mScriptableInfo))) {
        delete mScriptableInfo;
        mScriptableInfo = nullptr;
    }

    XPCWrappedNativeScope *scope = GetScope();
    if (scope) {
        Native2WrappedNativeMap* map = scope->GetWrappedNativeMap();

        // Post-1.9 we should not remove this wrapper from the map if it is
        // uninitialized.
        map->Remove(this);
    }

    if (mIdentity) {
        XPCJSRuntime* rt = GetRuntime();
        if (rt && rt->GetDoingFinalization()) {
            cyclecollector::DeferredFinalize(mIdentity);
            mIdentity = nullptr;
        } else {
            NS_RELEASE(mIdentity);
        }
    }

    mMaybeScope = nullptr;
}

void
XPCWrappedNative::UpdateScriptableInfo(XPCNativeScriptableInfo *si)
{
    MOZ_ASSERT(mScriptableInfo, "UpdateScriptableInfo expects an existing scriptable info");

    // Write barrier for incremental GC.
    JSRuntime* rt = GetRuntime()->Runtime();
    if (IsIncrementalBarrierNeeded(rt))
        mScriptableInfo->Mark();

    mScriptableInfo = si;
}

void
XPCWrappedNative::SetProto(XPCWrappedNativeProto* p)
{
    MOZ_ASSERT(!IsWrapperExpired(), "bad ptr!");

    MOZ_ASSERT(HasProto());

    // Write barrier for incremental GC.
    JSRuntime* rt = GetRuntime()->Runtime();
    GetProto()->WriteBarrierPre(rt);

    mMaybeProto = p;
}

// This is factored out so that it can be called publicly
// static
void
XPCWrappedNative::GatherProtoScriptableCreateInfo(nsIClassInfo* classInfo,
                                                  XPCNativeScriptableCreateInfo& sciProto)
{
    MOZ_ASSERT(classInfo, "bad param");
    MOZ_ASSERT(!sciProto.GetCallback(), "bad param");

    nsXPCClassInfo *classInfoHelper = nullptr;
    CallQueryInterface(classInfo, &classInfoHelper);
    if (classInfoHelper) {
        nsCOMPtr<nsIXPCScriptable> helper =
          dont_AddRef(static_cast<nsIXPCScriptable*>(classInfoHelper));
        uint32_t flags = classInfoHelper->GetScriptableFlags();
        sciProto.SetCallback(helper.forget());
        sciProto.SetFlags(XPCNativeScriptableFlags(flags));
        sciProto.SetInterfacesBitmap(classInfoHelper->GetInterfacesBitmap());

        return;
    }

    nsCOMPtr<nsISupports> possibleHelper;
    nsresult rv = classInfo->GetHelperForLanguage(nsIProgrammingLanguage::JAVASCRIPT,
                                                  getter_AddRefs(possibleHelper));
    if (NS_SUCCEEDED(rv) && possibleHelper) {
        nsCOMPtr<nsIXPCScriptable> helper(do_QueryInterface(possibleHelper));
        if (helper) {
            uint32_t flags = helper->GetScriptableFlags();
            sciProto.SetCallback(helper.forget());
            sciProto.SetFlags(XPCNativeScriptableFlags(flags));
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
    MOZ_ASSERT(!sciWrapper.GetCallback(), "bad param");

    // Get the class scriptable helper (if present)
    if (classInfo) {
        GatherProtoScriptableCreateInfo(classInfo, sciProto);

        if (sciProto.GetFlags().DontAskInstanceForScriptable())
            return sciProto;
    }

    // Do the same for the wrapper specific scriptable
    nsCOMPtr<nsIXPCScriptable> helper(do_QueryInterface(obj));
    if (helper) {
        uint32_t flags = helper->GetScriptableFlags();
        sciWrapper.SetCallback(helper.forget());
        sciWrapper.SetFlags(XPCNativeScriptableFlags(flags));

        // A whole series of assertions to catch bad uses of scriptable flags on
        // the siWrapper...

        MOZ_ASSERT(!(sciWrapper.GetFlags().WantPreCreate() &&
                     !sciProto.GetFlags().WantPreCreate()),
                   "Can't set WANT_PRECREATE on an instance scriptable "
                   "without also setting it on the class scriptable");

        MOZ_ASSERT(!(sciWrapper.GetFlags().DontEnumStaticProps() &&
                     !sciProto.GetFlags().DontEnumStaticProps() &&
                     sciProto.GetCallback()),
                   "Can't set DONT_ENUM_STATIC_PROPS on an instance scriptable "
                   "without also setting it on the class scriptable (if present and shared)");

        MOZ_ASSERT(!(sciWrapper.GetFlags().DontEnumQueryInterface() &&
                     !sciProto.GetFlags().DontEnumQueryInterface() &&
                     sciProto.GetCallback()),
                   "Can't set DONT_ENUM_QUERY_INTERFACE on an instance scriptable "
                   "without also setting it on the class scriptable (if present and shared)");

        MOZ_ASSERT(!(sciWrapper.GetFlags().DontAskInstanceForScriptable() &&
                     !sciProto.GetFlags().DontAskInstanceForScriptable()),
                   "Can't set DONT_ASK_INSTANCE_FOR_SCRIPTABLE on an instance scriptable "
                   "without also setting it on the class scriptable");

        MOZ_ASSERT(!(sciWrapper.GetFlags().ClassInfoInterfacesOnly() &&
                     !sciProto.GetFlags().ClassInfoInterfacesOnly() &&
                     sciProto.GetCallback()),
                   "Can't set CLASSINFO_INTERFACES_ONLY on an instance scriptable "
                   "without also setting it on the class scriptable (if present and shared)");

        MOZ_ASSERT(!(sciWrapper.GetFlags().AllowPropModsDuringResolve() &&
                     !sciProto.GetFlags().AllowPropModsDuringResolve() &&
                     sciProto.GetCallback()),
                   "Can't set ALLOW_PROP_MODS_DURING_RESOLVE on an instance scriptable "
                   "without also setting it on the class scriptable (if present and shared)");

        MOZ_ASSERT(!(sciWrapper.GetFlags().AllowPropModsToPrototype() &&
                     !sciProto.GetFlags().AllowPropModsToPrototype() &&
                     sciProto.GetCallback()),
                   "Can't set ALLOW_PROP_MODS_TO_PROTOTYPE on an instance scriptable "
                   "without also setting it on the class scriptable (if present and shared)");

        return sciWrapper;
    }

    return sciProto;
}

bool
XPCWrappedNative::Init(HandleObject parent,
                       const XPCNativeScriptableCreateInfo* sci)
{
    AutoJSContext cx;
    // setup our scriptable info...

    if (sci->GetCallback()) {
        if (HasProto()) {
            XPCNativeScriptableInfo* siProto = GetProto()->GetScriptableInfo();
            if (siProto && siProto->GetCallback() == sci->GetCallback())
                mScriptableInfo = siProto;
        }
        if (!mScriptableInfo) {
            mScriptableInfo =
                XPCNativeScriptableInfo::Construct(sci);

            if (!mScriptableInfo)
                return false;
        }
    }
    XPCNativeScriptableInfo* si = mScriptableInfo;

    // create our flatJSObject

    const JSClass* jsclazz = si ? si->GetJSClass() : Jsvalify(&XPC_WN_NoHelper_JSClass.base);

    // We should have the global jsclass flag if and only if we're a global.
    MOZ_ASSERT_IF(si, !!si->GetFlags().IsGlobalObject() == !!(jsclazz->flags & JSCLASS_IS_GLOBAL));

    MOZ_ASSERT(jsclazz &&
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

    RootedObject protoJSObject(cx, HasProto() ?
                                   GetProto()->GetJSProtoObject() :
                                   JS_GetObjectPrototype(cx, parent));
    if (!protoJSObject) {
        return false;
    }

    mFlatJSObject = JS_NewObject(cx, jsclazz, protoJSObject, parent);
    if (!mFlatJSObject) {
        mFlatJSObject.unsetFlags(FLAT_JS_OBJECT_VALID);
        return false;
    }

    mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);
    JS_SetPrivate(mFlatJSObject, this);

    return FinishInit();
}

bool
XPCWrappedNative::FinishInit()
{
    AutoJSContext cx;

    // This reference will be released when mFlatJSObject is finalized.
    // Since this reference will push the refcount to 2 it will also root
    // mFlatJSObject;
    MOZ_ASSERT(1 == mRefCnt, "unexpected refcount value");
    NS_ADDREF(this);

    if (mScriptableInfo && mScriptableInfo->GetFlags().WantCreate() &&
        NS_FAILED(mScriptableInfo->GetCallback()->Create(this, cx,
                                                         mFlatJSObject))) {
        return false;
    }

    // A hack for bug 517665, increase the probability for GC.
    JS_updateMallocCounter(cx, 2 * sizeof(XPCWrappedNative));

    return true;
}


NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCWrappedNative)
  NS_INTERFACE_MAP_ENTRY(nsIXPConnectWrappedNative)
  NS_INTERFACE_MAP_ENTRY(nsIXPConnectJSObjectHolder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPConnectWrappedNative)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPCWrappedNative)

// Release calls Destroy() immediately when the refcount drops to 0 to
// clear the weak references nsXPConnect has to XPCWNs and to ensure there
// are no pointers to dying protos.
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(XPCWrappedNative, Destroy())

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
                MOZ_ASSERT(JS_IsAboutToBeFinalizedUnbarriered(&jso));
                JS_SetPrivate(jso, nullptr);
                to->JSObjectFinalized();
            }

            // We also need to release any native pointers held...
            nsISupports* obj = to->GetNative();
            if (obj) {
#ifdef XP_WIN
                // Try to detect free'd pointer
                MOZ_ASSERT(*(int*)obj != 0xdddddddd, "bad pointer!");
                MOZ_ASSERT(*(int*)obj != 0,          "bad pointer!");
#endif
                XPCJSRuntime* rt = GetRuntime();
                if (rt) {
                    cyclecollector::DeferredFinalize(obj);
                } else {
                    obj->Release();
                }
                to->SetNative(nullptr);
            }

            to->SetInterface(nullptr);
        }
    }

    nsWrapperCache *cache = nullptr;
    CallQueryInterface(mIdentity, &cache);
    if (cache)
        cache->ClearWrapper();

    mFlatJSObject = nullptr;
    mFlatJSObject.unsetFlags(FLAT_JS_OBJECT_VALID);

    MOZ_ASSERT(mIdentity, "bad pointer!");
#ifdef XP_WIN
    // Try to detect free'd pointer
    MOZ_ASSERT(*(int*)mIdentity != 0xdddddddd, "bad pointer!");
    MOZ_ASSERT(*(int*)mIdentity != 0,          "bad pointer!");
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
    if (!IsValid())
        return;

    // The long standing strategy is to leak some objects still held at shutdown.
    // The general problem is that propagating release out of xpconnect at
    // shutdown time causes a world of problems.

    // We leak mIdentity (see above).

    // short circuit future finalization
    JS_SetPrivate(mFlatJSObject, nullptr);
    mFlatJSObject = nullptr;
    mFlatJSObject.unsetFlags(FLAT_JS_OBJECT_VALID);

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
                JS_SetPrivate(jso, nullptr);
                to->SetJSObject(nullptr);
            }
            // We leak the tearoff mNative
            // (for the same reason we leak mIdentity - see above).
            to->SetNative(nullptr);
            to->SetInterface(nullptr);
        }
    }

    if (mFirstChunk.mNextChunk) {
        delete mFirstChunk.mNextChunk;
        mFirstChunk.mNextChunk = nullptr;
    }
}

/***************************************************************************/

// Dynamically ensure that two objects don't end up with the same private.
class MOZ_STACK_CLASS AutoClonePrivateGuard {
public:
    AutoClonePrivateGuard(JSContext *cx, JSObject *aOld, JSObject *aNew)
        : mOldReflector(cx, aOld), mNewReflector(cx, aNew)
    {
        MOZ_ASSERT(JS_GetPrivate(aOld) == JS_GetPrivate(aNew));
    }

    ~AutoClonePrivateGuard()
    {
        if (JS_GetPrivate(mOldReflector)) {
            JS_SetPrivate(mNewReflector, nullptr);
        }
    }

private:
    RootedObject mOldReflector;
    RootedObject mNewReflector;
};

// static
nsresult
XPCWrappedNative::ReparentWrapperIfFound(XPCWrappedNativeScope* aOldScope,
                                         XPCWrappedNativeScope* aNewScope,
                                         HandleObject aNewParent,
                                         nsISupports* aCOMObj)
{
    // Check if we're near the stack limit before we get anywhere near the
    // transplanting code.
    AutoJSContext cx;
    JS_CHECK_RECURSION(cx, return NS_ERROR_FAILURE);

    XPCNativeInterface* iface = XPCNativeInterface::GetISupports();
    if (!iface)
        return NS_ERROR_FAILURE;

    nsresult rv;

    nsRefPtr<XPCWrappedNative> wrapper;
    RootedObject flat(cx);
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(aCOMObj, &cache);
    if (cache) {
        flat = cache->GetWrapper();
        if (flat) {
            wrapper = XPCWrappedNative::Get(flat);
            MOZ_ASSERT(wrapper->GetScope() == aOldScope,
                         "Incorrect scope passed");
        }
    } else {
        rv = XPCWrappedNative::GetUsedOnly(aCOMObj, aOldScope, iface,
                                           getter_AddRefs(wrapper));
        if (NS_FAILED(rv))
            return rv;

        if (wrapper)
            flat = wrapper->GetFlatJSObject();
    }

    if (!flat)
        return NS_OK;

    JSAutoCompartment ac(cx, aNewScope->GetGlobalJSObject());

    if (aOldScope != aNewScope) {
        // Oh, so now we need to move the wrapper to a different scope.
        AutoMarkingWrappedNativeProtoPtr oldProto(cx);
        AutoMarkingWrappedNativeProtoPtr newProto(cx);

        // Cross-scope means cross-compartment.
        MOZ_ASSERT(js::GetObjectCompartment(aOldScope->GetGlobalJSObject()) !=
                   js::GetObjectCompartment(aNewScope->GetGlobalJSObject()));
        MOZ_ASSERT(aNewParent, "won't be able to find the new parent");

        if (wrapper->HasProto()) {
            oldProto = wrapper->GetProto();
            XPCNativeScriptableInfo *info = oldProto->GetScriptableInfo();
            XPCNativeScriptableCreateInfo ci(*info);
            newProto =
                XPCWrappedNativeProto::GetNewOrUsed(aNewScope,
                                                    oldProto->GetClassInfo(),
                                                    &ci);
            if (!newProto) {
                return NS_ERROR_FAILURE;
            }
        }

        // First, the clone of the reflector, get a copy of its
        // properties and clone its expando chain. The only part that is
        // dangerous here if we have to return early is that we must avoid
        // ending up with two reflectors pointing to the same WN. Other than
        // that, the objects we create will just go away if we return early.

        RootedObject proto(cx, newProto->GetJSProtoObject());
        RootedObject newobj(cx, JS_CloneObject(cx, flat, proto, aNewParent));
        if (!newobj)
            return NS_ERROR_FAILURE;

        // At this point, both |flat| and |newobj| point to the same wrapped
        // native, which is bad, because one of them will end up finalizing
        // a wrapped native it does not own. |cloneGuard| ensures that if we
        // exit before calling clearing |flat|'s private the private of
        // |newobj| will be set to nullptr. |flat| will go away soon, because
        // we swap it with another object during the transplant and let that
        // object die.
        RootedObject propertyHolder(cx);
        {
            AutoClonePrivateGuard cloneGuard(cx, flat, newobj);

            propertyHolder = JS_NewObjectWithGivenProto(cx, nullptr, JS::NullPtr(),
                                                        aNewParent);
            if (!propertyHolder)
                return NS_ERROR_OUT_OF_MEMORY;
            if (!JS_CopyPropertiesFrom(cx, propertyHolder, flat))
                return NS_ERROR_FAILURE;

            // Expandos from other compartments are attached to the target JS object.
            // Copy them over, and let the old ones die a natural death.
            if (!XrayUtils::CloneExpandoChain(cx, newobj, flat))
                return NS_ERROR_FAILURE;

            // We've set up |newobj|, so we make it own the WN by nulling out
            // the private of |flat|.
            //
            // NB: It's important to do this _after_ copying the properties to
            // propertyHolder. Otherwise, an object with |foo.x === foo| will
            // crash when JS_CopyPropertiesFrom tries to call wrap() on foo.x.
            JS_SetPrivate(flat, nullptr);
        }

        // Update scope maps. This section modifies global state, so from
        // here on out we crash if anything fails.
        Native2WrappedNativeMap* oldMap = aOldScope->GetWrappedNativeMap();
        Native2WrappedNativeMap* newMap = aNewScope->GetWrappedNativeMap();

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

            MOZ_ASSERT(oldProto->GetScriptableInfo()->GetScriptableShared() ==
                       newProto->GetScriptableInfo()->GetScriptableShared(),
                       "Changing proto is also changing JSObject Classname or "
                       "helper's nsIXPScriptable flags. This is not allowed!");

            wrapper->UpdateScriptableInfo(newProto->GetScriptableInfo());
        }

        // Crash if the wrapper is already in the new scope.
        if (newMap->Find(wrapper->GetIdentityObject()))
            MOZ_CRASH();

        if (!newMap->Add(wrapper))
            MOZ_CRASH();

        flat = xpc::TransplantObject(cx, flat, newobj);
        if (!flat)
            MOZ_CRASH();

        MOZ_ASSERT(flat);
        wrapper->mFlatJSObject = flat;
        wrapper->mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

        if (cache) {
            bool preserving = cache->PreservingWrapper();
            cache->SetPreservingWrapper(false);
            cache->SetWrapper(flat);
            cache->SetPreservingWrapper(preserving);
        }
        if (!JS_CopyPropertiesFrom(cx, flat, propertyHolder))
            MOZ_CRASH();

        // Call the scriptable hook to indicate that we transplanted.
        XPCNativeScriptableInfo* si = wrapper->GetScriptableInfo();
        if (si->GetFlags().WantPostCreate())
            (void) si->GetCallback()->PostTransplant(wrapper, cx, flat);
    }

    // Now we can just fix up the parent and return the wrapper

    if (aNewParent) {
        if (!JS_SetParent(cx, flat, aNewParent))
            MOZ_CRASH();
    }

    return NS_OK;
}

// Orphans are sad little things - If only we could treat them better. :-(
//
// When a wrapper gets reparented to another scope (for example, when calling
// adoptNode), it's entirely possible that it previously served as the parent for
// other wrappers (via PreCreate hooks). When it moves, the old mFlatJSObject is
// replaced by a cross-compartment wrapper. Its descendants really _should_ move
// too, but we have no way of locating them short of a compartment-wide sweep
// (which we believe to be prohibitively expensive).
//
// So we just leave them behind. In practice, the only time this turns out to
// be a problem is during subsequent wrapper reparenting. When this happens, we
// call into the below fixup code at the last minute and straighten things out
// before proceeding.
//
// See bug 751995 for more information.

static nsresult
RescueOrphans(HandleObject obj)
{
    AutoJSContext cx;
    //
    // Even if we're not an orphan at the moment, one of our ancestors might
    // be. If so, we need to recursively rescue up the parent chain.
    //

    // First, get the parent object. If we're currently an orphan, the parent
    // object is a cross-compartment wrapper. Follow the parent into its own
    // compartment and fix it up there. We'll fix up |this| afterwards.
    //
    // NB: We pass stopAtOuter=false during the unwrap because Location objects
    // are parented to outer window proxies.
    nsresult rv;
    RootedObject parentObj(cx, js::GetObjectParent(obj));
    if (!parentObj)
        return NS_OK; // Global object. We're done.
    parentObj = js::UncheckedUnwrap(parentObj, /* stopAtOuter = */ false);

    // Recursively fix up orphans on the parent chain.
    rv = RescueOrphans(parentObj);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now that we know our parent is in the right place, determine if we've
    // been orphaned. If not, we have nothing to do.
    if (!js::IsCrossCompartmentWrapper(parentObj))
        return NS_OK;

    // We've been orphaned. Find where our parent went, and follow it.
    if (IS_WN_REFLECTOR(obj)) {
        RootedObject realParent(cx, js::UncheckedUnwrap(parentObj));
        XPCWrappedNative *wn =
            static_cast<XPCWrappedNative*>(js::GetObjectPrivate(obj));
        return wn->ReparentWrapperIfFound(ObjectScope(parentObj),
                                          ObjectScope(realParent),
                                          realParent, wn->GetIdentityObject());
    }

    JSAutoCompartment ac(cx, obj);
    return ReparentWrapper(cx, obj);
}

// Recursively fix up orphans on the parent chain of a wrapper. Note that this
// can cause a wrapper to move even if it is not an orphan, since its parent
// might be an orphan and fixing the parent causes this wrapper to become an
// orphan.
nsresult
XPCWrappedNative::RescueOrphans()
{
    AutoJSContext cx;
    RootedObject flatJSObject(cx, mFlatJSObject);
    return ::RescueOrphans(flatJSObject);
}

bool
XPCWrappedNative::ExtendSet(XPCNativeInterface* aInterface)
{
    AutoJSContext cx;

    if (!mSet->HasInterface(aInterface)) {
        AutoMarkingNativeSetPtr newSet(cx);
        newSet = XPCNativeSet::GetNewOrUsed(mSet, aInterface,
                                            mSet->GetInterfaceCount());
        if (!newSet)
            return false;

        mSet = newSet;
    }
    return true;
}

XPCWrappedNativeTearOff*
XPCWrappedNative::LocateTearOff(XPCNativeInterface* aInterface)
{
    for (XPCWrappedNativeTearOffChunk* chunk = &mFirstChunk;
         chunk != nullptr;
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
    return nullptr;
}

XPCWrappedNativeTearOff*
XPCWrappedNative::FindTearOff(XPCNativeInterface* aInterface,
                              bool needJSObject /* = false */,
                              nsresult* pError /* = nullptr */)
{
    AutoJSContext cx;
    nsresult rv = NS_OK;
    XPCWrappedNativeTearOff* to;
    XPCWrappedNativeTearOff* firstAvailable = nullptr;

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
                    AutoMarkingWrappedNativeTearOffPtr tearoff(cx, to);
                    bool ok = InitTearOffJSObject(to);
                    // During shutdown, we don't sweep tearoffs.  So make sure
                    // to unmark manually in case the auto-marker marked us.
                    // We shouldn't ever be getting here _during_ our
                    // Mark/Sweep cycle, so this should be safe.
                    to->Unmark();
                    if (!ok) {
                        to = nullptr;
                        rv = NS_ERROR_OUT_OF_MEMORY;
                    }
                }
                if (pError)
                    *pError = rv;
                return to;
            }
            if (!firstAvailable && to->IsAvailable())
                firstAvailable = to;
        }
    }

    to = firstAvailable;

    if (!to) {
        auto newChunk = new XPCWrappedNativeTearOffChunk();
        lastChunk->mNextChunk = newChunk;
        to = newChunk->mTearOffs;
    }

    {
        // Scope keeps |tearoff| from leaking across the rest of the function.
        AutoMarkingWrappedNativeTearOffPtr tearoff(cx, to);
        rv = InitTearOff(to, aInterface, needJSObject);
        // During shutdown, we don't sweep tearoffs.  So make sure to unmark
        // manually in case the auto-marker marked us.  We shouldn't ever be
        // getting here _during_ our Mark/Sweep cycle, so this should be safe.
        to->Unmark();
        if (NS_FAILED(rv))
            to = nullptr;
    }

    if (pError)
        *pError = rv;
    return to;
}

XPCWrappedNativeTearOff*
XPCWrappedNative::FindTearOff(const nsIID& iid) {
    AutoJSContext cx;
    AutoMarkingNativeInterfacePtr iface(cx);
    iface = XPCNativeInterface::GetNewOrUsed(&iid);
    return iface ? FindTearOff(iface) : nullptr;
}

nsresult
XPCWrappedNative::InitTearOff(XPCWrappedNativeTearOff* aTearOff,
                              XPCNativeInterface* aInterface,
                              bool needJSObject)
{
    AutoJSContext cx;

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

    // We are about to call out to other code.
    // So protect our intended tearoff.

    aTearOff->SetReserved();

    if (NS_FAILED(identity->QueryInterface(*iid, (void**)&obj)) || !obj) {
        aTearOff->SetInterface(nullptr);
        return NS_ERROR_NO_INTERFACE;
    }

    // Guard against trying to build a tearoff for a shared nsIClassInfo.
    if (iid->Equals(NS_GET_IID(nsIClassInfo))) {
        nsCOMPtr<nsISupports> alternate_identity(do_QueryInterface(obj));
        if (alternate_identity.get() != identity) {
            NS_RELEASE(obj);
            aTearOff->SetInterface(nullptr);
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
        RootedObject jso(cx, wrappedJS->GetJSObject());
        if (jso == mFlatJSObject) {
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

            NS_RELEASE(obj);
            aTearOff->SetInterface(nullptr);
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

        if (iid->Equals(NS_GET_IID(nsIPropertyBag)) && jso) {
            nsRefPtr<nsXPCWrappedJSClass> clasp = nsXPCWrappedJSClass::GetNewOrUsed(cx, *iid);
            if (clasp) {
                RootedObject answer(cx, clasp->CallQueryInterfaceOnJSObject(cx, jso, *iid));

                if (!answer) {
                    NS_RELEASE(obj);
                    aTearOff->SetInterface(nullptr);
                    return NS_ERROR_NO_INTERFACE;
                }
            }
        }
    }

    if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateWrapper(cx, *iid, identity,
                                                                   GetClassInfo()))) {
        // the security manager vetoed. It should have set an exception.
        NS_RELEASE(obj);
        aTearOff->SetInterface(nullptr);
        return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }

    // If this is not already in our set we need to extend our set.
    // Note: we do not cache the result of the previous call to HasInterface()
    // because we unlocked and called out in the interim and the result of the
    // previous call might not be correct anymore.

    if (!mSet->HasInterface(aInterface) && !ExtendSet(aInterface)) {
        NS_RELEASE(obj);
        aTearOff->SetInterface(nullptr);
        return NS_ERROR_NO_INTERFACE;
    }

    aTearOff->SetInterface(aInterface);
    aTearOff->SetNative(obj);
    if (needJSObject && !InitTearOffJSObject(aTearOff))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

bool
XPCWrappedNative::InitTearOffJSObject(XPCWrappedNativeTearOff* to)
{
    AutoJSContext cx;

    RootedObject parent(cx, mFlatJSObject);
    RootedObject proto(cx, JS_GetObjectPrototype(cx, parent));
    JSObject* obj = JS_NewObject(cx, Jsvalify(&XPC_WN_Tearoff_JSClass),
                                 proto, parent);
    if (!obj)
        return false;

    JS_SetPrivate(obj, to);
    to->SetJSObject(obj);
    return true;
}

/***************************************************************************/

static bool Throw(nsresult errNum, XPCCallContext& ccx)
{
    XPCThrower::Throw(errNum, ccx);
    return false;
}

/***************************************************************************/

class MOZ_STACK_CLASS CallMethodHelper
{
    XPCCallContext& mCallContext;
    // We wait to call SetLastResult(mInvokeResult) until ~CallMethodHelper(),
    // so that XPCWN-implemented functions like XPCComponents::GetLastResult()
    // can still access the previous result.
    nsresult mInvokeResult;
    nsIInterfaceInfo* const mIFaceInfo;
    const nsXPTMethodInfo* mMethodInfo;
    nsISupports* const mCallee;
    const uint16_t mVTableIndex;
    HandleId mIdxValueId;

    nsAutoTArray<nsXPTCVariant, 8> mDispatchParams;
    uint8_t mJSContextIndex; // TODO make const
    uint8_t mOptArgcIndex; // TODO make const

    jsval* const mArgv;
    const uint32_t mArgc;

    MOZ_ALWAYS_INLINE bool
    GetArraySizeFromParam(uint8_t paramIndex, uint32_t* result) const;

    MOZ_ALWAYS_INLINE bool
    GetInterfaceTypeFromParam(uint8_t paramIndex,
                              const nsXPTType& datum_type,
                              nsID* result) const;

    MOZ_ALWAYS_INLINE bool
    GetOutParamSource(uint8_t paramIndex, MutableHandleValue srcp) const;

    MOZ_ALWAYS_INLINE bool
    GatherAndConvertResults();

    MOZ_ALWAYS_INLINE bool
    QueryInterfaceFastPath();

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

    MOZ_ALWAYS_INLINE bool InitializeDispatchParams();

    MOZ_ALWAYS_INLINE bool ConvertIndependentParams(bool* foundDependentParam);
    MOZ_ALWAYS_INLINE bool ConvertIndependentParam(uint8_t i);
    MOZ_ALWAYS_INLINE bool ConvertDependentParams();
    MOZ_ALWAYS_INLINE bool ConvertDependentParam(uint8_t i);

    MOZ_ALWAYS_INLINE void CleanupParam(nsXPTCMiniVariant& param, nsXPTType& type);

    MOZ_ALWAYS_INLINE bool AllocateStringClass(nsXPTCVariant* dp,
                                               const nsXPTParamInfo& paramInfo);

    MOZ_ALWAYS_INLINE nsresult Invoke();

public:

    CallMethodHelper(XPCCallContext& ccx)
        : mCallContext(ccx)
        , mInvokeResult(NS_ERROR_UNEXPECTED)
        , mIFaceInfo(ccx.GetInterface()->GetInterfaceInfo())
        , mMethodInfo(nullptr)
        , mCallee(ccx.GetTearOff()->GetNative())
        , mVTableIndex(ccx.GetMethodIndex())
        , mIdxValueId(ccx.GetRuntime()->GetStringID(XPCJSRuntime::IDX_VALUE))
        , mJSContextIndex(UINT8_MAX)
        , mOptArgcIndex(UINT8_MAX)
        , mArgv(ccx.GetArgv())
        , mArgc(ccx.GetArgc())

    {
        // Success checked later.
        mIFaceInfo->GetMethodInfo(mVTableIndex, &mMethodInfo);
    }

    ~CallMethodHelper();

    MOZ_ALWAYS_INLINE bool Call();

};

// static
bool
XPCWrappedNative::CallMethod(XPCCallContext& ccx,
                             CallMode mode /*= CALL_METHOD */)
{
    MOZ_ASSERT(ccx.GetXPCContext()->CallerTypeIsJavaScript(),
               "Native caller for XPCWrappedNative::CallMethod?");

    nsresult rv = ccx.CanCallNow();
    if (NS_FAILED(rv)) {
        return Throw(rv, ccx);
    }

    return CallMethodHelper(ccx).Call();
}

bool
CallMethodHelper::Call()
{
    mCallContext.SetRetVal(JSVAL_VOID);

    XPCJSRuntime::Get()->SetPendingException(nullptr);

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
    bool foundDependentParam = false;
    if (!ConvertIndependentParams(&foundDependentParam))
        return false;

    if (foundDependentParam && !ConvertDependentParams())
        return false;

    mInvokeResult = Invoke();

    if (JS_IsExceptionPending(mCallContext)) {
        return false;
    }

    if (NS_FAILED(mInvokeResult)) {
        ThrowBadResult(mInvokeResult, mCallContext);
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

    mCallContext.GetXPCContext()->SetLastResult(mInvokeResult);
}

bool
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

bool
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

bool
CallMethodHelper::GetOutParamSource(uint8_t paramIndex, MutableHandleValue srcp) const
{
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(paramIndex);

    if ((paramInfo.IsOut() || paramInfo.IsDipper()) &&
        !paramInfo.IsRetval()) {
        MOZ_ASSERT(paramIndex < mArgc || paramInfo.IsOptional(),
                   "Expected either enough arguments or an optional argument");
        jsval arg = paramIndex < mArgc ? mArgv[paramIndex] : JSVAL_NULL;
        if (paramIndex < mArgc) {
            RootedObject obj(mCallContext);
            if (!arg.isPrimitive())
                obj = &arg.toObject();
            if (!obj || !JS_GetPropertyById(mCallContext, obj, mIdxValueId, srcp)) {
                // Explicitly passed in unusable value for out param.  Note
                // that if i >= mArgc we already know that |arg| is JSVAL_NULL,
                // and that's ok.
                ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, paramIndex,
                              mCallContext);
                return false;
            }
        }
    }

    return true;
}

bool
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
        RootedValue v(mCallContext, NullValue());
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
            if (!XPCConvert::NativeArray2JS(&v, (const void**)&dp->val,
                                            datum_type, &param_iid,
                                            array_count, &err)) {
                // XXX need exception scheme for arrays to indicate bad element
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        } else if (isSizedString) {
            if (!XPCConvert::NativeStringWithSize2JS(&v,
                                                     (const void*)&dp->val,
                                                     datum_type,
                                                     array_count, &err)) {
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        } else {
            if (!XPCConvert::NativeData2JS(&v, &dp->val, datum_type,
                                           &param_iid, &err)) {
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        }

        if (paramInfo.IsRetval()) {
            mCallContext.SetRetVal(v);
        } else if (i < mArgc) {
            // we actually assured this before doing the invoke
            MOZ_ASSERT(mArgv[i].isObject(), "out var is not object");
            RootedObject obj(mCallContext, &mArgv[i].toObject());
            if (!JS_SetPropertyById(mCallContext, obj, mIdxValueId, v)) {
                ThrowBadParam(NS_ERROR_XPC_CANT_SET_OUT_VAL, i, mCallContext);
                return false;
            }
        } else {
            MOZ_ASSERT(paramInfo.IsOptional(),
                       "Expected either enough arguments or an optional argument");
        }
    }

    return true;
}

bool
CallMethodHelper::QueryInterfaceFastPath()
{
    MOZ_ASSERT(mVTableIndex == 0,
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

    nsISupports* qiresult = nullptr;
    mInvokeResult = mCallee->QueryInterface(*iid, (void**) &qiresult);

    if (NS_FAILED(mInvokeResult)) {
        ThrowBadResult(mInvokeResult, mCallContext);
        return false;
    }

    RootedValue v(mCallContext, NullValue());
    nsresult err;
    bool success =
        XPCConvert::NativeData2JS(&v, &qiresult,
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

bool
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
        dp->val.p = nullptr;
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
        dp->val.u8 = std::min<uint32_t>(mArgc, paramCount) - requiredArgs;
    }

    return true;
}

bool
CallMethodHelper::ConvertIndependentParams(bool* foundDependentParam)
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

bool
CallMethodHelper::ConvertIndependentParam(uint8_t i)
{
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
    const nsXPTType& type = paramInfo.GetType();
    uint8_t type_tag = type.TagPart();
    nsXPTCVariant* dp = GetDispatchParam(i);
    dp->type = type;
    MOZ_ASSERT(!paramInfo.IsShared(), "[shared] implies [noscript]!");

    // String classes are always "in" - those that are marked "out" are converted
    // by the XPIDL compiler to "in+dipper". See the note above IsDipper() in
    // xptinfo.h.
    //
    // Also note that the fact that we bail out early for dipper parameters means
    // that "inout" dipper parameters don't work - see bug 687612.
    if (paramInfo.IsStringClass()) {
        if (!AllocateStringClass(dp, paramInfo))
            return false;
        if (paramInfo.IsDipper())
            return true;
    }

    // Specify the correct storage/calling semantics.
    if (paramInfo.IsIndirect())
        dp->SetIndirect();

    // The JSVal proper is always stored within the 'val' union and passed
    // indirectly, regardless of in/out-ness.
    if (type_tag == nsXPTType::T_JSVAL) {
        // Root the value.
        dp->val.j = JSVAL_VOID;
        if (!js::AddRawValueRoot(mCallContext, &dp->val.j, "XPCWrappedNative::CallMethod param"))
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
    RootedValue src(mCallContext);
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
        MOZ_ASSERT(i < mArgc || paramInfo.IsOptional(),
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

    nsresult err;
    if (!XPCConvert::JSData2Native(&dp->val, src, type, &param_iid, &err)) {
        ThrowBadParam(err, i, mCallContext);
        return false;
    }

    return true;
}

bool
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

bool
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
        MOZ_ASSERT(datum_type.TagPart() != nsXPTType::T_JSVAL,
                   "Arrays of JSVals not currently supported - see bug 693337.");
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
    RootedValue src(mCallContext);
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
        MOZ_ASSERT(i < mArgc || paramInfo.IsOptional(),
                     "Expected either enough arguments or an optional argument");
        src = i < mArgc ? mArgv[i] : JSVAL_NULL;
    }

    nsID param_iid;
    if (datum_type.IsInterfacePointer() &&
        !GetInterfaceTypeFromParam(i, datum_type, &param_iid))
        return false;

    nsresult err;

    if (isArray || isSizedString) {
        if (!GetArraySizeFromParam(i, &array_count))
            return false;

        if (isArray) {
            if (array_count &&
                !XPCConvert::JSArray2Native((void**)&dp->val, src,
                                            array_count, datum_type, &param_iid,
                                            &err)) {
                // XXX need exception scheme for arrays to indicate bad element
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        } else // if (isSizedString)
        {
            if (!XPCConvert::JSStringWithSize2Native((void*)&dp->val,
                                                     src, array_count,
                                                     datum_type, &err)) {
                ThrowBadParam(err, i, mCallContext);
                return false;
            }
        }
    } else {
        if (!XPCConvert::JSData2Native(&dp->val, src, type,
                                       &param_iid, &err)) {
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
    MOZ_ASSERT(type.TagPart() != nsXPTType::T_ARRAY, "Can't handle arrays.");

    // Pointers may sometimes be null even if cleanup was requested. Combine
    // the null checking for all the different types into one check here.
    if (type.TagPart() != nsXPTType::T_JSVAL && param.val.p == nullptr)
        return;

    switch (type.TagPart()) {
        case nsXPTType::T_JSVAL:
            js::RemoveRawValueRoot(mCallContext, (jsval*)&param.val);
            break;
        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            ((nsISupports*)param.val.p)->Release();
            break;
        case nsXPTType::T_ASTRING:
        case nsXPTType::T_DOMSTRING:
            nsXPConnect::GetRuntimeInstance()->mScratchStrings.Destroy((nsString*)param.val.p);
            break;
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
            nsXPConnect::GetRuntimeInstance()->mScratchCStrings.Destroy((nsCString*)param.val.p);
            break;
        default:
            MOZ_ASSERT(!type.IsArithmetic(), "Cleanup requested on unexpected type.");
            nsMemory::Free(param.val.p);
            break;
    }
}

bool
CallMethodHelper::AllocateStringClass(nsXPTCVariant* dp,
                                      const nsXPTParamInfo& paramInfo)
{
    // Get something we can make comparisons with.
    uint8_t type_tag = paramInfo.GetType().TagPart();

    // There should be 4 cases, all strings. Verify that here.
    MOZ_ASSERT(type_tag == nsXPTType::T_ASTRING ||
               type_tag == nsXPTType::T_DOMSTRING ||
               type_tag == nsXPTType::T_UTF8STRING ||
               type_tag == nsXPTType::T_CSTRING,
               "Unexpected string class type!");

    // ASTRING and DOMSTRING are very similar, and both use nsString.
    // UTF8_STRING and CSTRING are also quite similar, and both use nsCString.
    if (type_tag == nsXPTType::T_ASTRING || type_tag == nsXPTType::T_DOMSTRING)
        dp->val.p = nsXPConnect::GetRuntimeInstance()->mScratchStrings.Create();
    else
        dp->val.p = nsXPConnect::GetRuntimeInstance()->mScratchCStrings.Create();

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
    uint32_t argc = mDispatchParams.Length();
    nsXPTCVariant* argv = mDispatchParams.Elements();

    return NS_InvokeByIndex(mCallee, mVTableIndex, argc, argv);
}

/***************************************************************************/
// interface methods

/* JSObjectPtr GetJSObject(); */
JSObject*
XPCWrappedNative::GetJSObject()
{
    return GetFlatJSObject();
}

/* readonly attribute nsISupports Native; */
NS_IMETHODIMP XPCWrappedNative::GetNative(nsISupports * *aNative)
{
    // No need to QI here, we already have the correct nsISupports
    // vtable.
    nsCOMPtr<nsISupports> rval = mIdentity;
    rval.forget(aNative);
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
        MOZ_ASSERT(equal, "Principal mismatch.  Expect bad things to happen");
    }
#endif
    return principal;
}

/* XPCNativeInterface FindInterfaceWithMember (in JSHandleId name); */
NS_IMETHODIMP XPCWrappedNative::FindInterfaceWithMember(HandleId name,
                                                        nsIInterfaceInfo * *_retval)
{
    XPCNativeInterface* iface;
    XPCNativeMember*  member;

    if (GetSet()->FindMember(name, &member, &iface) && iface) {
        nsCOMPtr<nsIInterfaceInfo> temp = iface->GetInterfaceInfo();
        temp.forget(_retval);
    } else
        *_retval = nullptr;
    return NS_OK;
}

/* XPCNativeInterface FindInterfaceWithName (in JSHandleId name); */
NS_IMETHODIMP XPCWrappedNative::FindInterfaceWithName(HandleId name,
                                                      nsIInterfaceInfo * *_retval)
{
    XPCNativeInterface* iface = GetSet()->FindNamedInterface(name);
    if (iface) {
        nsCOMPtr<nsIInterfaceInfo> temp = iface->GetInterfaceInfo();
        temp.forget(_retval);
    } else
        *_retval = nullptr;
    return NS_OK;
}

/* [notxpcom] bool HasNativeMember (in JSHandleId name); */
NS_IMETHODIMP_(bool)
XPCWrappedNative::HasNativeMember(HandleId name)
{
    XPCNativeMember *member = nullptr;
    uint16_t ignored;
    return GetSet()->FindMember(name, &member, &ignored) && !!member;
}

/* void finishInitForWrappedGlobal (); */
NS_IMETHODIMP XPCWrappedNative::FinishInitForWrappedGlobal()
{
    // We can only be called under certain conditions.
    MOZ_ASSERT(mScriptableInfo);
    MOZ_ASSERT(mScriptableInfo->GetFlags().IsGlobalObject());
    MOZ_ASSERT(HasProto());

    // Call PostCreateProrotype.
    bool success = GetProto()->CallPostCreatePrototype();
    if (!success)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

/* void debugDump (in short depth); */
NS_IMETHODIMP XPCWrappedNative::DebugDump(int16_t depth)
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

        XPC_LOG_ALWAYS(("mFlatJSObject of %x", mFlatJSObject.getPtr()));
        XPC_LOG_ALWAYS(("mIdentity of %x", mIdentity));
        XPC_LOG_ALWAYS(("mScriptableInfo @ %x", mScriptableInfo));

        if (depth && mScriptableInfo) {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mScriptable @ %x", mScriptableInfo->GetCallback()));
            XPC_LOG_ALWAYS(("mFlags of %x", (uint32_t)mScriptableInfo->GetFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %x", mScriptableInfo->GetJSClass()));
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

/***************************************************************************/

char*
XPCWrappedNative::ToString(XPCWrappedNativeTearOff* to /* = nullptr */ ) const
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

    char* sz = nullptr;
    char* name = nullptr;

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
        uint16_t count = set->GetInterfaceCount();

        if (count == 1)
            name = JS_sprintf_append(name, "%s", array[0]->GetNameString());
        else if (count == 2 &&
                 array[0] == XPCNativeInterface::GetISupports()) {
            name = JS_sprintf_append(name, "%s", array[1]->GetNameString());
        } else {
            for (uint16_t i = 0; i < count; i++) {
                const char* fmt = (i == 0) ?
                                    "(%s" : (i == count-1) ?
                                        ", %s)" : ", %s";
                name = JS_sprintf_append(name, fmt,
                                         array[i]->GetNameString());
            }
        }
    }

    if (!name) {
        return nullptr;
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
    uint16_t count = set->GetInterfaceCount();
    for (uint16_t i = 0; i < count; i++) {
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

        char* className = nullptr;
        char* contractID = nullptr;
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

        if (className)
            nsMemory::Free(className);
        if (contractID)
            nsMemory::Free(contractID);
    }
}
#endif

NS_IMPL_ISUPPORTS(XPCJSObjectHolder, nsIXPConnectJSObjectHolder)

JSObject*
XPCJSObjectHolder::GetJSObject()
{
    NS_PRECONDITION(mJSObj, "bad object state");
    return mJSObj;
}

XPCJSObjectHolder::XPCJSObjectHolder(JSObject* obj)
    : mJSObj(obj)
{
    XPCJSRuntime::Get()->AddObjectHolderRoot(this);
}

XPCJSObjectHolder::~XPCJSObjectHolder()
{
    RemoveFromRootSet();
}

void
XPCJSObjectHolder::TraceJS(JSTracer *trc)
{
    trc->setTracingDetails(GetTraceName, this, 0);
    JS_CallObjectTracer(trc, &mJSObj, "XPCJSObjectHolder::mJSObj");
}

// static
void
XPCJSObjectHolder::GetTraceName(JSTracer* trc, char *buf, size_t bufsize)
{
    JS_snprintf(buf, bufsize, "XPCJSObjectHolder[0x%p].mJSObj",
                trc->debugPrintArg());
}

// static
XPCJSObjectHolder*
XPCJSObjectHolder::newHolder(JSObject* obj)
{
    if (!obj) {
        NS_ERROR("bad param");
        return nullptr;
    }
    return new XPCJSObjectHolder(obj);
}
