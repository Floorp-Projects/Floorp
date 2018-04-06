/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Wrapper object for reflecting native xpcom objects into JavaScript. */

#include "xpcprivate.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsWrapperCacheInlines.h"
#include "XPCLog.h"
#include "js/Printf.h"
#include "jsfriendapi.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "XrayWrapper.h"

#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"

#include <stdint.h>
#include "mozilla/DeferredFinalize.h"
#include "mozilla/Likely.h"
#include "mozilla/Unused.h"
#include "mozilla/Sprintf.h"
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
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XPCWrappedNative)
    tmp->ExpireWrapper();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(XPCWrappedNative)
    if (!tmp->IsValid())
        return NS_OK;

    if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
        char name[72];
        nsCOMPtr<nsIXPCScriptable> scr = tmp->GetScriptable();
        if (scr)
            SprintfLiteral(name, "XPCWrappedNative (%s)",
                           scr->GetJSClass()->name);
        else
            SprintfLiteral(name, "XPCWrappedNative");

        cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
    } else {
        NS_IMPL_CYCLE_COLLECTION_DESCRIBE(XPCWrappedNative, tmp->mRefCnt.get())
    }

    if (tmp->HasExternalReference()) {

        // If our refcount is > 1, our reference to the flat JS object is
        // considered "strong", and we're going to traverse it.
        //
        // If our refcount is <= 1, our reference to the flat JS object is
        // considered "weak", and we're *not* going to traverse it.
        //
        // This reasoning is in line with the slightly confusing lifecycle rules
        // for XPCWrappedNatives, described in a larger comment below and also
        // on our wiki at http://wiki.mozilla.org/XPConnect_object_wrapping

        JSObject* obj = tmp->GetFlatJSObjectPreserveColor();
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mFlatJSObject");
        cb.NoteJSChild(JS::GCCellPtr(obj));
    }

    // XPCWrappedNative keeps its native object alive.
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mIdentity");
    cb.NoteXPCOMChild(tmp->GetIdentityObject());

    tmp->NoteTearoffs(cb);

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void
XPCWrappedNative::Suspect(nsCycleCollectionNoteRootCallback& cb)
{
    if (!IsValid() || IsWrapperExpired())
        return;

    MOZ_ASSERT(NS_IsMainThread(),
               "Suspecting wrapped natives from non-main thread");

    // Only record objects that might be part of a cycle as roots, unless
    // the callback wants all traces (a debug feature). Do this even if
    // the XPCWN doesn't own the JS reflector object in case the reflector
    // keeps alive other C++ things. This is safe because if the reflector
    // had died the reference from the XPCWN to it would have been cleared.
    JSObject* obj = GetFlatJSObjectPreserveColor();
    if (JS::ObjectIsMarkedGray(obj) || cb.WantAllTraces())
        cb.NoteJSRoot(obj);
}

void
XPCWrappedNative::NoteTearoffs(nsCycleCollectionTraversalCallback& cb)
{
    // Tearoffs hold their native object alive. If their JS object hasn't been
    // finalized yet we'll note the edge between the JS object and the native
    // (see nsXPConnect::Traverse), but if their JS object has been finalized
    // then the tearoff is only reachable through the XPCWrappedNative, so we
    // record an edge here.
    for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to; to = to->GetNextTearOff()) {
        JSObject* jso = to->GetJSObjectPreserveColor();
        if (!jso) {
            NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "tearoff's mNative");
            cb.NoteXPCOMChild(to->GetNative());
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
             nsWrapperCache* cache,
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
XPCWrappedNative::WrapNewGlobal(xpcObjectHelper& nativeHelper,
                                nsIPrincipal* principal,
                                bool initStandardClasses,
                                JS::CompartmentOptions& aOptions,
                                XPCWrappedNative** wrappedGlobal)
{
    AutoJSContext cx;
    nsISupports* identity = nativeHelper.GetCanonical();

    // The object should specify that it's meant to be global.
    MOZ_ASSERT(nativeHelper.GetScriptableFlags() & XPC_SCRIPTABLE_IS_GLOBAL_OBJECT);

    // We shouldn't be reusing globals.
    MOZ_ASSERT(!nativeHelper.GetWrapperCache() ||
               !nativeHelper.GetWrapperCache()->GetWrapperPreserveColor());

    // Get the nsIXPCScriptable. This will tell us the JSClass of the object
    // we're going to create.
    nsCOMPtr<nsIXPCScriptable> scrProto;
    nsCOMPtr<nsIXPCScriptable> scrWrapper;
    GatherScriptable(identity, nativeHelper.GetClassInfo(),
                     getter_AddRefs(scrProto), getter_AddRefs(scrWrapper));
    MOZ_ASSERT(scrWrapper);

    // Finally, we get to the JSClass.
    const JSClass* clasp = scrWrapper->GetJSClass();
    MOZ_ASSERT(clasp->flags & JSCLASS_IS_GLOBAL);

    // Create the global.
    aOptions.creationOptions().setTrace(XPCWrappedNative::Trace);
    if (xpc::SharedMemoryEnabled())
        aOptions.creationOptions().setSharedMemoryAndAtomicsEnabled(true);
    RootedObject global(cx, xpc::CreateGlobalObject(cx, clasp, principal, aOptions));
    if (!global)
        return NS_ERROR_FAILURE;
    XPCWrappedNativeScope* scope = RealmPrivate::Get(global)->scope;

    // Immediately enter the global's compartment, so that everything else we
    // create ends up there.
    JSAutoCompartment ac(cx, global);

    // If requested, initialize the standard classes on the global.
    if (initStandardClasses && ! JS_InitStandardClasses(cx, global))
        return NS_ERROR_FAILURE;

    // Make a proto.
    XPCWrappedNativeProto* proto =
        XPCWrappedNativeProto::GetNewOrUsed(scope,
                                            nativeHelper.GetClassInfo(),
                                            scrProto,
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
    RefPtr<XPCWrappedNative> wrapper =
        new XPCWrappedNative(nativeHelper.forgetCanonical(), proto);

    //
    // We don't call ::Init() on this wrapper, because our setup requirements
    // are different for globals. We do our setup inline here, instead.
    //

    wrapper->mScriptable = scrWrapper;

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
    RefPtr<XPCNativeInterface> iface = XPCNativeInterface::GetNewOrUsed(&NS_GET_IID(nsISupports));
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
    nsWrapperCache* cache = helper.GetWrapperCache();

    MOZ_ASSERT(!cache || !cache->GetWrapperPreserveColor(),
               "We assume the caller already checked if it could get the "
               "wrapper from the cache.");

    nsresult rv;

    MOZ_ASSERT(!Scope->GetRuntime()->GCIsRunning(),
               "XPCWrappedNative::GetNewOrUsed called during GC");

    nsISupports* identity = helper.GetCanonical();

    if (!identity) {
        NS_ERROR("This XPCOM object fails in QueryInterface to nsISupports!");
        return NS_ERROR_FAILURE;
    }

    RefPtr<XPCWrappedNative> wrapper;

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

    nsIClassInfo* info = helper.GetClassInfo();

    nsCOMPtr<nsIXPCScriptable> scrProto;
    nsCOMPtr<nsIXPCScriptable> scrWrapper;

    // Gather scriptable create info if we are wrapping something
    // other than an nsIClassInfo object. We need to not do this for
    // nsIClassInfo objects because often nsIClassInfo implementations
    // are also nsIXPCScriptable helper implementations, but the helper
    // code is obviously intended for the implementation of the class
    // described by the nsIClassInfo, not for the class info object
    // itself.
    if (!isClassInfoSingleton)
        GatherScriptable(identity, info, getter_AddRefs(scrProto),
                         getter_AddRefs(scrWrapper));

    RootedObject parent(cx, Scope->GetGlobalJSObject());

    mozilla::Maybe<JSAutoCompartment> ac;

    if (scrWrapper && scrWrapper->WantPreCreate()) {
        RootedObject plannedParent(cx, parent);
        nsresult rv =
            scrWrapper->PreCreate(identity, cx, parent, parent.address());
        if (NS_FAILED(rv))
            return rv;
        rv = NS_OK;

        MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(parent),
                   "Xray wrapper being used to parent XPCWrappedNative?");

        MOZ_ASSERT(js::GetGlobalForObjectCrossCompartment(parent) == parent,
                   "Non-global being used to parent XPCWrappedNative?");

        ac.emplace(static_cast<JSContext*>(cx), parent);

        if (parent != plannedParent) {
            XPCWrappedNativeScope* betterScope = ObjectScope(parent);
            MOZ_ASSERT(betterScope != Scope,
                       "How can we have the same scope for two different globals?");
            return GetNewOrUsed(helper, betterScope, Interface, resultWrapper);
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
        proto = XPCWrappedNativeProto::GetNewOrUsed(Scope, info, scrProto);
        if (!proto)
            return NS_ERROR_FAILURE;

        wrapper = new XPCWrappedNative(helper.forgetCanonical(), proto);
    } else {
        RefPtr<XPCNativeInterface> iface = Interface;
        if (!iface)
            iface = XPCNativeInterface::GetISupports();

        XPCNativeSetKey key(iface);
        RefPtr<XPCNativeSet> set =
            XPCNativeSet::GetNewOrUsed(&key);

        if (!set)
            return NS_ERROR_FAILURE;

        wrapper = new XPCWrappedNative(helper.forgetCanonical(), Scope,
                                       set.forget());
    }

    MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(parent),
               "Xray wrapper being used to parent XPCWrappedNative?");

    // We use an AutoMarkingPtr here because it is possible for JS gc to happen
    // after we have Init'd the wrapper but *before* we add it to the hashtable.
    // This would cause the mSet to get collected and we'd later crash. I've
    // *seen* this happen.
    AutoMarkingWrappedNativePtr wrapperMarker(cx, wrapper);

    if (!wrapper->Init(scrWrapper))
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
             nsWrapperCache* cache,
             XPCWrappedNative* inWrapper,
             XPCWrappedNative** resultWrapper)
{
    AutoJSContext cx;
    MOZ_ASSERT(inWrapper);

    Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();

    RefPtr<XPCWrappedNative> wrapper;
    // Deal with the case where the wrapper got created as a side effect
    // of one of our calls out of this code. Add() returns the (possibly
    // pre-existing) wrapper that ultimately ends up in the map, which is
    // what we want.
    wrapper = map->Add(inWrapper);
    if (!wrapper)
        return NS_ERROR_FAILURE;

    if (wrapper == inWrapper) {
        JSObject* flat = wrapper->GetFlatJSObject();
        MOZ_ASSERT(!cache || !cache->GetWrapperPreserveColor() ||
                   flat == cache->GetWrapperPreserveColor(),
                   "This object has a cached wrapper that's different from "
                   "the JSObject held by its native wrapper?");

        if (cache && !cache->GetWrapperPreserveColor())
            cache->SetWrapper(flat);
    }

    DEBUG_CheckClassInfoClaims(wrapper);
    wrapper.forget(resultWrapper);
    return NS_OK;
}

// This ctor is used if this object will have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                                   XPCWrappedNativeProto* aProto)
    : mMaybeProto(aProto),
      mSet(aProto->GetSet())
{
    MOZ_ASSERT(NS_IsMainThread());

    mIdentity = aIdentity;
    mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

    MOZ_ASSERT(mMaybeProto, "bad ctor param");
    MOZ_ASSERT(mSet, "bad ctor param");
}

// This ctor is used if this object will NOT have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                                   XPCWrappedNativeScope* aScope,
                                   already_AddRefed<XPCNativeSet>&& aSet)

    : mMaybeScope(TagScope(aScope)),
      mSet(aSet)
{
    MOZ_ASSERT(NS_IsMainThread());

    mIdentity = aIdentity;
    mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

    MOZ_ASSERT(aScope, "bad ctor param");
    MOZ_ASSERT(mSet, "bad ctor param");
}

XPCWrappedNative::~XPCWrappedNative()
{
    Destroy();
}

void
XPCWrappedNative::Destroy()
{
    mScriptable = nullptr;

#ifdef DEBUG
    // Check that this object has already been swept from the map.
    XPCWrappedNativeScope* scope = GetScope();
    if (scope) {
        Native2WrappedNativeMap* map = scope->GetWrappedNativeMap();
        MOZ_ASSERT(map->Find(GetIdentityObject()) != this);
    }
#endif

    if (mIdentity) {
        XPCJSRuntime* rt = GetRuntime();
        if (rt && rt->GetDoingFinalization()) {
            DeferredFinalize(mIdentity.forget().take());
        } else {
            mIdentity = nullptr;
        }
    }

    mMaybeScope = nullptr;
}

// This is factored out so that it can be called publicly.
// static
nsIXPCScriptable*
XPCWrappedNative::GatherProtoScriptable(nsIClassInfo* classInfo)
{
    MOZ_ASSERT(classInfo, "bad param");

    nsXPCClassInfo* classInfoHelper = nullptr;
    CallQueryInterface(classInfo, &classInfoHelper);
    if (classInfoHelper) {
        nsCOMPtr<nsIXPCScriptable> helper =
          dont_AddRef(static_cast<nsIXPCScriptable*>(classInfoHelper));
        return helper;
    }

    nsCOMPtr<nsIXPCScriptable> helper;
    nsresult rv = classInfo->GetScriptableHelper(getter_AddRefs(helper));
    if (NS_SUCCEEDED(rv) && helper) {
        return helper;
    }

    return nullptr;
}

// static
void
XPCWrappedNative::GatherScriptable(nsISupports* aObj,
                                   nsIClassInfo* aClassInfo,
                                   nsIXPCScriptable** aScrProto,
                                   nsIXPCScriptable** aScrWrapper)
{
    MOZ_ASSERT(!*aScrProto, "bad param");
    MOZ_ASSERT(!*aScrWrapper, "bad param");

    nsCOMPtr<nsIXPCScriptable> scrProto;
    nsCOMPtr<nsIXPCScriptable> scrWrapper;

    // Get the class scriptable helper (if present)
    if (aClassInfo) {
        scrProto = GatherProtoScriptable(aClassInfo);

        if (scrProto && scrProto->DontAskInstanceForScriptable()) {
            scrWrapper = scrProto;
            scrProto.forget(aScrProto);
            scrWrapper.forget(aScrWrapper);
            return;
        }
    }

    // Do the same for the wrapper specific scriptable
    scrWrapper = do_QueryInterface(aObj);
    if (scrWrapper) {
        // A whole series of assertions to catch bad uses of scriptable flags on
        // the scrWrapper...

        // Can't set WANT_PRECREATE on an instance scriptable without also
        // setting it on the class scriptable.
        MOZ_ASSERT_IF(scrWrapper->WantPreCreate(),
                      scrProto && scrProto->WantPreCreate());

        // Can't set DONT_ENUM_QUERY_INTERFACE on an instance scriptable
        // without also setting it on the class scriptable (if present).
        MOZ_ASSERT_IF(scrWrapper->DontEnumQueryInterface() && scrProto,
                      scrProto->DontEnumQueryInterface());

        // Can't set DONT_ASK_INSTANCE_FOR_SCRIPTABLE on an instance scriptable
        // without also setting it on the class scriptable.
        MOZ_ASSERT_IF(scrWrapper->DontAskInstanceForScriptable(),
                      scrProto && scrProto->DontAskInstanceForScriptable());

        // Can't set CLASSINFO_INTERFACES_ONLY on an instance scriptable
        // without also setting it on the class scriptable (if present).
        MOZ_ASSERT_IF(scrWrapper->ClassInfoInterfacesOnly() && scrProto,
                      scrProto->ClassInfoInterfacesOnly());

        // Can't set ALLOW_PROP_MODS_DURING_RESOLVE on an instance scriptable
        // without also setting it on the class scriptable (if present).
        MOZ_ASSERT_IF(scrWrapper->AllowPropModsDuringResolve() && scrProto,
                      scrProto->AllowPropModsDuringResolve());

        // Can't set ALLOW_PROP_MODS_TO_PROTOTYPE on an instance scriptable
        // without also setting it on the class scriptable (if present).
        MOZ_ASSERT_IF(scrWrapper->AllowPropModsToPrototype() && scrProto,
                      scrProto->AllowPropModsToPrototype());
    } else {
        scrWrapper = scrProto;
    }

    scrProto.forget(aScrProto);
    scrWrapper.forget(aScrWrapper);
}

bool
XPCWrappedNative::Init(nsIXPCScriptable* aScriptable)
{
    AutoJSContext cx;

    // Setup our scriptable...
    MOZ_ASSERT(!mScriptable);
    mScriptable = aScriptable;

    // create our flatJSObject

    const JSClass* jsclazz = mScriptable
                           ? mScriptable->GetJSClass()
                           : Jsvalify(&XPC_WN_NoHelper_JSClass);

    // We should have the global jsclass flag if and only if we're a global.
    MOZ_ASSERT_IF(mScriptable, !!mScriptable->IsGlobalObject() ==
                               !!(jsclazz->flags & JSCLASS_IS_GLOBAL));

    MOZ_ASSERT(jsclazz &&
               jsclazz->name &&
               jsclazz->flags &&
               jsclazz->getResolve() &&
               jsclazz->hasFinalize(), "bad class");

    // XXXbz JS_GetObjectPrototype wants an object, even though it then asserts
    // that this object is same-compartment with cx, which means it could just
    // use the cx global...
    RootedObject global(cx, CurrentGlobalOrNull(cx));
    RootedObject protoJSObject(cx, HasProto() ?
                                   GetProto()->GetJSProtoObject() :
                                   JS_GetObjectPrototype(cx, global));
    if (!protoJSObject) {
        return false;
    }

    mFlatJSObject = JS_NewObjectWithGivenProto(cx, jsclazz, protoJSObject);
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

    for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to; to = to->GetNextTearOff()) {
        JSObject* jso = to->GetJSObjectPreserveColor();
        if (jso) {
            JS_SetPrivate(jso, nullptr);
#ifdef DEBUG
            JS_UpdateWeakPointerAfterGCUnbarriered(&jso);
            MOZ_ASSERT(!jso);
#endif
            to->JSObjectFinalized();
        }

        // We also need to release any native pointers held...
        RefPtr<nsISupports> native = to->TakeNative();
        if (native && GetRuntime()) {
            DeferredFinalize(native.forget().take());
        }

        to->SetInterface(nullptr);
    }

    nsWrapperCache* cache = nullptr;
    CallQueryInterface(mIdentity, &cache);
    if (cache)
        cache->ClearWrapper(mFlatJSObject.unbarrieredGetPtr());

    mFlatJSObject = nullptr;
    mFlatJSObject.unsetFlags(FLAT_JS_OBJECT_VALID);

    MOZ_ASSERT(mIdentity, "bad pointer!");
#ifdef XP_WIN
    // Try to detect free'd pointer
    MOZ_ASSERT(*(int*)mIdentity.get() != (int)0xdddddddd, "bad pointer!");
    MOZ_ASSERT(*(int*)mIdentity.get() != (int)0,          "bad pointer!");
#endif

    if (IsWrapperExpired()) {
        Destroy();
    }

    // Note that it's not safe to touch mNativeWrapper here since it's
    // likely that it has already been finalized.

    Release();
}

void
XPCWrappedNative::FlatJSObjectMoved(JSObject* obj, const JSObject* old)
{
    JS::AutoAssertGCCallback inCallback;
    MOZ_ASSERT(mFlatJSObject == old);

    nsWrapperCache* cache = nullptr;
    CallQueryInterface(mIdentity, &cache);
    if (cache)
        cache->UpdateWrapper(obj, old);

    mFlatJSObject = obj;
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

    // Short circuit future finalization.
    JS_SetPrivate(mFlatJSObject, nullptr);
    mFlatJSObject = nullptr;
    mFlatJSObject.unsetFlags(FLAT_JS_OBJECT_VALID);

    XPCWrappedNativeProto* proto = GetProto();

    if (HasProto())
        proto->SystemIsBeingShutDown();

    // We don't clear mScriptable here. The destructor will do it.

    // Cleanup the tearoffs.
    for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to; to = to->GetNextTearOff()) {
        if (JSObject* jso = to->GetJSObjectPreserveColor()) {
            JS_SetPrivate(jso, nullptr);
            to->SetJSObject(nullptr);
        }
        // We leak the tearoff mNative
        // (for the same reason we leak mIdentity - see above).
        Unused << to->TakeNative().take();
        to->SetInterface(nullptr);
    }
}

/***************************************************************************/

// Dynamically ensure that two objects don't end up with the same private.
class MOZ_STACK_CLASS AutoClonePrivateGuard {
public:
    AutoClonePrivateGuard(JSContext* cx, JSObject* aOld, JSObject* aNew)
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

bool
XPCWrappedNative::ExtendSet(XPCNativeInterface* aInterface)
{
    if (!mSet->HasInterface(aInterface)) {
        XPCNativeSetKey key(mSet, aInterface);
        RefPtr<XPCNativeSet> newSet =
            XPCNativeSet::GetNewOrUsed(&key);
        if (!newSet)
            return false;

        mSet = newSet.forget();
    }
    return true;
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

    XPCWrappedNativeTearOff* lastTearOff;
    for (lastTearOff = to = &mFirstTearOff;
         to;
         lastTearOff = to, to = to->GetNextTearOff()) {
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

    to = firstAvailable;

    if (!to) {
        to = lastTearOff->AddTearOff();
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
    RefPtr<XPCNativeInterface> iface = XPCNativeInterface::GetNewOrUsed(&iid);
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

    // This is an nsRefPtr instead of an nsCOMPtr because it may not be the
    // canonical nsISupports for this object.
    RefPtr<nsISupports> qiResult;

    // If the scriptable helper forbids us from reflecting additional
    // interfaces, then don't even try the QI, just fail.
    if (mScriptable &&
        mScriptable->ClassInfoInterfacesOnly() &&
        !mSet->HasInterface(aInterface) &&
        !mSet->HasInterfaceWithAncestor(aInterface)) {
        return NS_ERROR_NO_INTERFACE;
    }

    // We are about to call out to other code.
    // So protect our intended tearoff.

    aTearOff->SetReserved();

    if (NS_FAILED(identity->QueryInterface(*iid, getter_AddRefs(qiResult))) || !qiResult) {
        aTearOff->SetInterface(nullptr);
        return NS_ERROR_NO_INTERFACE;
    }

    // Guard against trying to build a tearoff for a shared nsIClassInfo.
    if (iid->Equals(NS_GET_IID(nsIClassInfo))) {
        nsCOMPtr<nsISupports> alternate_identity(do_QueryInterface(qiResult));
        if (alternate_identity.get() != identity) {
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

    nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(qiResult));
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
            RefPtr<nsXPCWrappedJSClass> clasp = nsXPCWrappedJSClass::GetNewOrUsed(cx, *iid);
            if (clasp) {
                RootedObject answer(cx, clasp->CallQueryInterfaceOnJSObject(cx, jso, *iid));

                if (!answer) {
                    aTearOff->SetInterface(nullptr);
                    return NS_ERROR_NO_INTERFACE;
                }
            }
        }
    }

    if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateWrapper(cx, *iid, identity,
                                                                   GetClassInfo()))) {
        // the security manager vetoed. It should have set an exception.
        aTearOff->SetInterface(nullptr);
        return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }

    // If this is not already in our set we need to extend our set.
    // Note: we do not cache the result of the previous call to HasInterface()
    // because we unlocked and called out in the interim and the result of the
    // previous call might not be correct anymore.

    if (!mSet->HasInterface(aInterface) && !ExtendSet(aInterface)) {
        aTearOff->SetInterface(nullptr);
        return NS_ERROR_NO_INTERFACE;
    }

    aTearOff->SetInterface(aInterface);
    aTearOff->SetNative(qiResult);
    if (needJSObject && !InitTearOffJSObject(aTearOff))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

bool
XPCWrappedNative::InitTearOffJSObject(XPCWrappedNativeTearOff* to)
{
    AutoJSContext cx;

    JSObject* obj = JS_NewObject(cx, Jsvalify(&XPC_WN_Tearoff_JSClass));
    if (!obj)
        return false;

    JS_SetPrivate(obj, to);
    to->SetJSObject(obj);

    js::SetReservedSlot(obj, XPC_WN_TEAROFF_FLAT_OBJECT_SLOT,
                        JS::ObjectValue(*mFlatJSObject));
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
    nsresult mInvokeResult;
    const nsXPTInterfaceInfo* const mIFaceInfo;
    const nsXPTMethodInfo* mMethodInfo;
    nsISupports* const mCallee;
    const uint16_t mVTableIndex;
    HandleId mIdxValueId;

    AutoTArray<nsXPTCVariant, 8> mDispatchParams;
    uint8_t mJSContextIndex; // TODO make const
    uint8_t mOptArgcIndex; // TODO make const

    Value* const mArgv;
    const uint32_t mArgc;

    MOZ_ALWAYS_INLINE bool
    GetArraySizeFromParam(uint8_t paramIndex, HandleValue maybeArray, uint32_t* result);

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

    explicit CallMethodHelper(XPCCallContext& ccx)
        : mCallContext(ccx)
        , mInvokeResult(NS_ERROR_UNEXPECTED)
        , mIFaceInfo(ccx.GetInterface()->GetInterfaceInfo())
        , mMethodInfo(nullptr)
        , mCallee(ccx.GetTearOff()->GetNative())
        , mVTableIndex(ccx.GetMethodIndex())
        , mIdxValueId(ccx.GetContext()->GetStringID(XPCJSContext::IDX_VALUE))
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
    nsresult rv = ccx.CanCallNow();
    if (NS_FAILED(rv)) {
        return Throw(rv, ccx);
    }

    return CallMethodHelper(ccx).Call();
}

bool
CallMethodHelper::Call()
{
    mCallContext.SetRetVal(JS::UndefinedValue());

    mCallContext.GetContext()->SetPendingException(nullptr);

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
                    if (!GetArraySizeFromParam(i, UndefinedHandleValue, &array_count) ||
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
                free(p);
            } else {
                // Clean up single parameters (if requested).
                if (dp->DoesValNeedCleanup())
                    CleanupParam(*dp, dp->type);
            }
        }
    }
}

bool
CallMethodHelper::GetArraySizeFromParam(uint8_t paramIndex,
                                        HandleValue maybeArray,
                                        uint32_t* result)
{
    nsresult rv;
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(paramIndex);

    // TODO fixup the various exceptions that are thrown

    rv = mIFaceInfo->GetSizeIsArgNumberForParam(mVTableIndex, &paramInfo, 0, &paramIndex);
    if (NS_FAILED(rv))
        return Throw(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, mCallContext);

    // If the array length wasn't passed, it might have been listed as optional.
    // When converting arguments from JS to C++, we pass the array as |maybeArray|,
    // and give ourselves the chance to infer the length. Once we have it, we stick
    // it in the right slot so that we can find it again when cleaning up the params.
    // from the array.
    if (paramIndex >= mArgc && maybeArray.isObject()) {
        MOZ_ASSERT(mMethodInfo->GetParam(paramIndex).IsOptional());
        RootedObject arrayOrNull(mCallContext, maybeArray.isObject() ? &maybeArray.toObject()
                                                                     : nullptr);

        bool isArray;
        if (!JS_IsArrayObject(mCallContext, maybeArray, &isArray) ||
            !isArray ||
            !JS_GetArrayLength(mCallContext, arrayOrNull, &GetDispatchParam(paramIndex)->val.u32))
        {
            return Throw(NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY, mCallContext);
        }
    }

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
    bool isRetval = &paramInfo == mMethodInfo->GetRetval();

    MOZ_ASSERT(!paramInfo.IsDipper(), "Dipper params are handled separately");
    if (paramInfo.IsOut() && !isRetval) {
        MOZ_ASSERT(paramIndex < mArgc || paramInfo.IsOptional(),
                   "Expected either enough arguments or an optional argument");
        Value arg = paramIndex < mArgc ? mArgv[paramIndex] : JS::NullValue();
        if (paramIndex < mArgc) {
            RootedObject obj(mCallContext);
            if (!arg.isPrimitive())
                obj = &arg.toObject();
            if (!obj || !JS_GetPropertyById(mCallContext, obj, mIdxValueId, srcp)) {
                // Explicitly passed in unusable value for out param.  Note
                // that if i >= mArgc we already know that |arg| is JS::NullValue(),
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
            if (!GetArraySizeFromParam(i, UndefinedHandleValue, &array_count))
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

        if (&paramInfo == mMethodInfo->GetRetval()) {
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
                                  { nsXPTType::T_INTERFACE_IS },
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
    if (mMethodInfo->HasRetval()) {
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
        if (paramInfo.IsDipper()) {
            // We've allocated our string class explicitly, so we don't need
            // to do any conversions on the incoming argument. However, we still
            // need to verify that it's an object, so that we don't get surprised
            // later on when trying to assign the result to .value.
            if (i < mArgc && !mArgv[i].isObject()) {
                ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, i, mCallContext);
                return false;
            }
            return true;
        }
    }

    // Specify the correct storage/calling semantics.
    if (paramInfo.IsIndirect())
        dp->SetIndirect();

    // The JSVal proper is always stored within the 'val' union and passed
    // indirectly, regardless of in/out-ness.
    if (type_tag == nsXPTType::T_JSVAL) {
        // Root the value.
        dp->val.j.asValueRef().setUndefined();
        if (!js::AddRawValueRoot(mCallContext, &dp->val.j.asValueRef(),
                                 "XPCWrappedNative::CallMethod param"))
        {
            return false;
        }
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
            src.setUndefined();
        else
            src.setNull();
    }

    nsID param_iid;
    if (type_tag == nsXPTType::T_INTERFACE &&
        NS_FAILED(mIFaceInfo->GetIIDForParamNoAlloc(mVTableIndex, &paramInfo,
                                                    &param_iid))) {
        ThrowBadParam(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO, i, mCallContext);
        return false;
    }

    // Don't allow CPOWs to be passed to native code (in case they try to cast
    // to a concrete type).
    if (src.isObject() &&
        jsipc::IsWrappedCPOW(&src.toObject()) &&
        type_tag == nsXPTType::T_INTERFACE &&
        !param_iid.Equals(NS_GET_IID(nsISupports)))
    {
        // Allow passing CPOWs to XPCWrappedJS.
        nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(mCallee));
        if (!wrappedJS) {
            ThrowBadParam(NS_ERROR_XPC_CANT_PASS_CPOW_TO_NATIVE, i, mCallContext);
            return false;
        }
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
        src = i < mArgc ? mArgv[i] : JS::NullValue();
    }

    nsID param_iid;
    if (datum_type.IsInterfacePointer() &&
        !GetInterfaceTypeFromParam(i, datum_type, &param_iid))
        return false;

    nsresult err;

    if (isArray || isSizedString) {
        if (!GetArraySizeFromParam(i, src, &array_count))
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
            js::RemoveRawValueRoot(mCallContext, (Value*)&param.val);
            break;
        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            ((nsISupports*)param.val.p)->Release();
            break;
        case nsXPTType::T_DOMOBJECT:
            type.GetDOMObjectInfo().Cleanup(param.val.p);
            break;
        case nsXPTType::T_ASTRING:
        case nsXPTType::T_DOMSTRING:
            mCallContext.GetContext()->mScratchStrings.Destroy((nsString*)param.val.p);
            break;
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
            mCallContext.GetContext()->mScratchCStrings.Destroy((nsCString*)param.val.p);
            break;
        default:
            MOZ_ASSERT(!type.IsArithmetic(), "Cleanup requested on unexpected type.");
            free(param.val.p);
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
        dp->val.p = mCallContext.GetContext()->mScratchStrings.Create();
    else
        dp->val.p = mCallContext.GetContext()->mScratchCStrings.Create();

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

JSObject*
XPCWrappedNative::GetJSObject()
{
    return GetFlatJSObject();
}

NS_IMETHODIMP XPCWrappedNative::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNative @ %p with mRefCnt = %" PRIuPTR, this, mRefCnt.get()));
    XPC_LOG_INDENT();

        if (HasProto()) {
            XPCWrappedNativeProto* proto = GetProto();
            if (depth && proto)
                proto->DebugDump(depth);
            else
                XPC_LOG_ALWAYS(("mMaybeProto @ %p", proto));
        } else
            XPC_LOG_ALWAYS(("Scope @ %p", GetScope()));

        if (depth && mSet)
            mSet->DebugDump(depth);
        else
            XPC_LOG_ALWAYS(("mSet @ %p", mSet.get()));

        XPC_LOG_ALWAYS(("mFlatJSObject of %p", mFlatJSObject.unbarrieredGetPtr()));
        XPC_LOG_ALWAYS(("mIdentity of %p", mIdentity.get()));
        XPC_LOG_ALWAYS(("mScriptable @ %p", mScriptable.get()));

        if (depth && mScriptable) {
            XPC_LOG_INDENT();
            XPC_LOG_ALWAYS(("mFlags of %x", mScriptable->GetScriptableFlags()));
            XPC_LOG_ALWAYS(("mJSClass @ %p", mScriptable->GetJSClass()));
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

    UniqueChars sz;
    UniqueChars name;

    nsCOMPtr<nsIXPCScriptable> scr = GetScriptable();
    if (scr)
        name = JS_smprintf("%s", scr->GetJSClass()->name);
    if (to) {
        const char* fmt = name ? " (%s)" : "%s";
        name = JS_sprintf_append(Move(name), fmt,
                                 to->GetInterface()->GetNameString());
    } else if (!name) {
        XPCNativeSet* set = GetSet();
        XPCNativeInterface** array = set->GetInterfaceArray();
        RefPtr<XPCNativeInterface> isupp = XPCNativeInterface::GetISupports();
        uint16_t count = set->GetInterfaceCount();

        if (count == 1)
            name = JS_sprintf_append(Move(name), "%s", array[0]->GetNameString());
        else if (count == 2 && array[0] == isupp) {
            name = JS_sprintf_append(Move(name), "%s", array[1]->GetNameString());
        } else {
            for (uint16_t i = 0; i < count; i++) {
                const char* fmt = (i == 0) ?
                                    "(%s" : (i == count-1) ?
                                        ", %s)" : ", %s";
                name = JS_sprintf_append(Move(name), fmt,
                                         array[i]->GetNameString());
            }
        }
    }

    if (!name) {
        return nullptr;
    }
    const char* fmt = "[xpconnect wrapped %s" FMT_ADDR FMT_STR(" (native")
        FMT_ADDR FMT_STR(")") "]";
    if (scr) {
        fmt = "[object %s" FMT_ADDR FMT_STR(" (native") FMT_ADDR FMT_STR(")") "]";
    }
    sz = JS_smprintf(fmt, name.get() PARAM_ADDR(this) PARAM_ADDR(mIdentity.get()));

    return sz.release();

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
        const nsXPTInterfaceInfo* info = iface->GetInterfaceInfo();
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
        if (wrapper->GetScriptable()) {
            wrapper->GetScriptable()->GetClassName(&className);
        }

        printf("\n!!! Object's nsIClassInfo lies about its interfaces!!!\n"
               "   classname: %s \n"
               "   contractid: %s \n"
               "   unimplemented interface name: %s\n\n",
               className ? className : "<unknown>",
               contractID ? contractID : "<unknown>",
               interfaceName);

        if (className)
            free(className);
        if (contractID)
            free(contractID);
    }
}
#endif
