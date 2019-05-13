/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Wrapper object for reflecting native xpcom objects into JavaScript. */

#include "xpcprivate.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsWrapperCacheInlines.h"
#include "XPCLog.h"
#include "js/MemoryFunctions.h"
#include "js/Printf.h"
#include "jsfriendapi.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "XrayWrapper.h"

#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"

#include <new>
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
  if (!tmp->IsValid()) {
    return NS_OK;
  }

  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[72];
    nsCOMPtr<nsIXPCScriptable> scr = tmp->GetScriptable();
    if (scr) {
      SprintfLiteral(name, "XPCWrappedNative (%s)", scr->GetJSClass()->name);
    } else {
      SprintfLiteral(name, "XPCWrappedNative");
    }

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

void XPCWrappedNative::Suspect(nsCycleCollectionNoteRootCallback& cb) {
  if (!IsValid() || IsWrapperExpired()) {
    return;
  }

  MOZ_ASSERT(NS_IsMainThread(),
             "Suspecting wrapped natives from non-main thread");

  // Only record objects that might be part of a cycle as roots, unless
  // the callback wants all traces (a debug feature). Do this even if
  // the XPCWN doesn't own the JS reflector object in case the reflector
  // keeps alive other C++ things. This is safe because if the reflector
  // had died the reference from the XPCWN to it would have been cleared.
  JSObject* obj = GetFlatJSObjectPreserveColor();
  if (JS::ObjectIsMarkedGray(obj) || cb.WantAllTraces()) {
    cb.NoteJSRoot(obj);
  }
}

void XPCWrappedNative::NoteTearoffs(nsCycleCollectionTraversalCallback& cb) {
  // Tearoffs hold their native object alive. If their JS object hasn't been
  // finalized yet we'll note the edge between the JS object and the native
  // (see nsXPConnect::Traverse), but if their JS object has been finalized
  // then the tearoff is only reachable through the XPCWrappedNative, so we
  // record an edge here.
  for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to;
       to = to->GetNextTearOff()) {
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
#  define DEBUG_CheckClassInfoClaims(wrapper) ((void)0)
#endif

/***************************************************************************/
static nsresult FinishCreate(JSContext* cx, XPCWrappedNativeScope* Scope,
                             XPCNativeInterface* Interface,
                             nsWrapperCache* cache, XPCWrappedNative* inWrapper,
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
nsresult XPCWrappedNative::WrapNewGlobal(JSContext* cx,
                                         xpcObjectHelper& nativeHelper,
                                         nsIPrincipal* principal,
                                         bool initStandardClasses,
                                         JS::RealmOptions& aOptions,
                                         XPCWrappedNative** wrappedGlobal) {
  nsCOMPtr<nsISupports> identity = do_QueryInterface(nativeHelper.Object());

  // The object should specify that it's meant to be global.
  MOZ_ASSERT(nativeHelper.GetScriptableFlags() &
             XPC_SCRIPTABLE_IS_GLOBAL_OBJECT);

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
  xpc::SetPrefableRealmOptions(aOptions);

  RootedObject global(cx,
                      xpc::CreateGlobalObject(cx, clasp, principal, aOptions));
  if (!global) {
    return NS_ERROR_FAILURE;
  }
  XPCWrappedNativeScope* scope = ObjectScope(global);

  // Immediately enter the global's realm, so that everything else we
  // create ends up there.
  JSAutoRealm ar(cx, global);

  // If requested, initialize the standard classes on the global.
  if (initStandardClasses && !JS::InitRealmStandardClasses(cx)) {
    return NS_ERROR_FAILURE;
  }

  // Make a proto.
  XPCWrappedNativeProto* proto = XPCWrappedNativeProto::GetNewOrUsed(
      cx, scope, nativeHelper.GetClassInfo(), scrProto);
  if (!proto) {
    return NS_ERROR_FAILURE;
  }

  // Set up the prototype on the global.
  MOZ_ASSERT(proto->GetJSProtoObject());
  RootedObject protoObj(cx, proto->GetJSProtoObject());
  bool success = JS_SplicePrototype(cx, global, protoObj);
  if (!success) {
    return NS_ERROR_FAILURE;
  }

  // Construct the wrapper, which takes over the strong reference to the
  // native object.
  RefPtr<XPCWrappedNative> wrapper =
      new XPCWrappedNative(identity.forget(), proto);

  //
  // We don't call ::Init() on this wrapper, because our setup requirements
  // are different for globals. We do our setup inline here, instead.
  //

  wrapper->mScriptable = scrWrapper;

  // Set the JS object to the global we already created.
  wrapper->SetFlatJSObject(global);

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
  success = wrapper->FinishInit(cx);
  MOZ_ASSERT(success);

  // Go through some extra work to find the tearoff. This is kind of silly
  // on a conceptual level: the point of tearoffs is to cache the results
  // of QI-ing mIdentity to different interfaces, and we don't need that
  // since we're dealing with nsISupports. But lots of code expects tearoffs
  // to exist for everything, so we just follow along.
  RefPtr<XPCNativeInterface> iface =
      XPCNativeInterface::GetNewOrUsed(cx, &NS_GET_IID(nsISupports));
  MOZ_ASSERT(iface);
  nsresult status;
  success = wrapper->FindTearOff(cx, iface, false, &status);
  if (!success) {
    return status;
  }

  // Call the common creation finish routine. This does all of the bookkeeping
  // like inserting the wrapper into the wrapper map and setting up the wrapper
  // cache.
  nsresult rv = FinishCreate(cx, scope, iface, nativeHelper.GetWrapperCache(),
                             wrapper, wrappedGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
nsresult XPCWrappedNative::GetNewOrUsed(JSContext* cx, xpcObjectHelper& helper,
                                        XPCWrappedNativeScope* Scope,
                                        XPCNativeInterface* Interface,
                                        XPCWrappedNative** resultWrapper) {
  MOZ_ASSERT(Interface);
  nsWrapperCache* cache = helper.GetWrapperCache();

  MOZ_ASSERT(!cache || !cache->GetWrapperPreserveColor(),
             "We assume the caller already checked if it could get the "
             "wrapper from the cache.");

  nsresult rv;

  MOZ_ASSERT(!Scope->GetRuntime()->GCIsRunning(),
             "XPCWrappedNative::GetNewOrUsed called during GC");

  nsCOMPtr<nsISupports> identity = do_QueryInterface(helper.Object());

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
    if (!wrapper->FindTearOff(cx, Interface, false, &rv)) {
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
  bool isClassInfoSingleton =
      helper.GetClassInfo() == helper.Object() &&
      NS_SUCCEEDED(helper.GetClassInfo()->GetFlags(&classInfoFlags)) &&
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
  if (!isClassInfoSingleton) {
    GatherScriptable(identity, info, getter_AddRefs(scrProto),
                     getter_AddRefs(scrWrapper));
  }

  RootedObject parent(cx, Scope->GetGlobalForWrappedNatives());

  mozilla::Maybe<JSAutoRealm> ar;

  if (scrWrapper && scrWrapper->WantPreCreate()) {
    RootedObject plannedParent(cx, parent);
    nsresult rv = scrWrapper->PreCreate(identity, cx, parent, parent.address());
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_OK;

    MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(parent),
               "Xray wrapper being used to parent XPCWrappedNative?");

    MOZ_ASSERT(JS_IsGlobalObject(parent),
               "Non-global being used to parent XPCWrappedNative?");

    ar.emplace(static_cast<JSContext*>(cx), parent);

    if (parent != plannedParent) {
      XPCWrappedNativeScope* betterScope = ObjectScope(parent);
      MOZ_ASSERT(betterScope != Scope,
                 "How can we have the same scope for two different globals?");
      return GetNewOrUsed(cx, helper, betterScope, Interface, resultWrapper);
    }

    // Take the performance hit of checking the hashtable again in case
    // the preCreate call caused the wrapper to get created through some
    // interesting path (the DOM code tends to make this happen sometimes).

    if (cache) {
      RootedObject cached(cx, cache->GetWrapper());
      if (cached) {
        wrapper = XPCWrappedNative::Get(cached);
      }
    } else {
      wrapper = map->Find(identity);
    }

    if (wrapper) {
      if (!wrapper->FindTearOff(cx, Interface, false, &rv)) {
        MOZ_ASSERT(NS_FAILED(rv), "returning NS_OK on failure");
        return rv;
      }
      wrapper.forget(resultWrapper);
      return NS_OK;
    }
  } else {
    ar.emplace(static_cast<JSContext*>(cx), parent);
  }

  AutoMarkingWrappedNativeProtoPtr proto(cx);

  // If there is ClassInfo (and we are not building a wrapper for the
  // nsIClassInfo interface) then we use a wrapper that needs a prototype.

  // Note that the security check happens inside FindTearOff - after the
  // wrapper is actually created, but before JS code can see it.

  if (info && !isClassInfoSingleton) {
    proto = XPCWrappedNativeProto::GetNewOrUsed(cx, Scope, info, scrProto);
    if (!proto) {
      return NS_ERROR_FAILURE;
    }

    wrapper = new XPCWrappedNative(identity.forget(), proto);
  } else {
    RefPtr<XPCNativeInterface> iface = Interface;
    if (!iface) {
      iface = XPCNativeInterface::GetISupports(cx);
    }

    XPCNativeSetKey key(cx, iface);
    RefPtr<XPCNativeSet> set = XPCNativeSet::GetNewOrUsed(cx, &key);

    if (!set) {
      return NS_ERROR_FAILURE;
    }

    wrapper = new XPCWrappedNative(identity.forget(), Scope, set.forget());
  }

  MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(parent),
             "Xray wrapper being used to parent XPCWrappedNative?");

  // We use an AutoMarkingPtr here because it is possible for JS gc to happen
  // after we have Init'd the wrapper but *before* we add it to the hashtable.
  // This would cause the mSet to get collected and we'd later crash. I've
  // *seen* this happen.
  AutoMarkingWrappedNativePtr wrapperMarker(cx, wrapper);

  if (!wrapper->Init(cx, scrWrapper)) {
    return NS_ERROR_FAILURE;
  }

  if (!wrapper->FindTearOff(cx, Interface, false, &rv)) {
    MOZ_ASSERT(NS_FAILED(rv), "returning NS_OK on failure");
    return rv;
  }

  return FinishCreate(cx, Scope, Interface, cache, wrapper, resultWrapper);
}

static nsresult FinishCreate(JSContext* cx, XPCWrappedNativeScope* Scope,
                             XPCNativeInterface* Interface,
                             nsWrapperCache* cache, XPCWrappedNative* inWrapper,
                             XPCWrappedNative** resultWrapper) {
  MOZ_ASSERT(inWrapper);

  Native2WrappedNativeMap* map = Scope->GetWrappedNativeMap();

  RefPtr<XPCWrappedNative> wrapper;
  // Deal with the case where the wrapper got created as a side effect
  // of one of our calls out of this code. Add() returns the (possibly
  // pre-existing) wrapper that ultimately ends up in the map, which is
  // what we want.
  wrapper = map->Add(inWrapper);
  if (!wrapper) {
    return NS_ERROR_FAILURE;
  }

  if (wrapper == inWrapper) {
    JSObject* flat = wrapper->GetFlatJSObject();
    MOZ_ASSERT(!cache || !cache->GetWrapperPreserveColor() ||
                   flat == cache->GetWrapperPreserveColor(),
               "This object has a cached wrapper that's different from "
               "the JSObject held by its native wrapper?");

    if (cache && !cache->GetWrapperPreserveColor()) {
      cache->SetWrapper(flat);
    }
  }

  DEBUG_CheckClassInfoClaims(wrapper);
  wrapper.forget(resultWrapper);
  return NS_OK;
}

// This ctor is used if this object will have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                                   XPCWrappedNativeProto* aProto)
    : mMaybeProto(aProto), mSet(aProto->GetSet()) {
  MOZ_ASSERT(NS_IsMainThread());

  mIdentity = aIdentity;
  RecordReplayRegisterDeferredFinalizeThing(nullptr, nullptr, mIdentity);

  mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

  MOZ_ASSERT(mMaybeProto, "bad ctor param");
  MOZ_ASSERT(mSet, "bad ctor param");
}

// This ctor is used if this object will NOT have a proto.
XPCWrappedNative::XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                                   XPCWrappedNativeScope* aScope,
                                   already_AddRefed<XPCNativeSet>&& aSet)

    : mMaybeScope(TagScope(aScope)), mSet(aSet) {
  MOZ_ASSERT(NS_IsMainThread());

  mIdentity = aIdentity;
  RecordReplayRegisterDeferredFinalizeThing(nullptr, nullptr, mIdentity);

  mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);

  MOZ_ASSERT(aScope, "bad ctor param");
  MOZ_ASSERT(mSet, "bad ctor param");
}

XPCWrappedNative::~XPCWrappedNative() { Destroy(); }

void XPCWrappedNative::Destroy() {
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
    // Either release mIdentity immediately or defer the release. When
    // recording or replaying the release must always be deferred, so that
    // DeferredFinalize matches the earlier call to
    // RecordReplayRegisterDeferredFinalizeThing.
    XPCJSRuntime* rt = GetRuntime();
    if ((rt && rt->GetDoingFinalization()) ||
        recordreplay::IsRecordingOrReplaying()) {
      DeferredFinalize(mIdentity.forget().take());
    } else {
      mIdentity = nullptr;
    }
  }

  mMaybeScope = nullptr;
}

// A hack for bug 517665, increase the probability for GC.
// TODO: Try removing this and just using the actual size of the object.
static const size_t GCMemoryFactor = 2;

inline void XPCWrappedNative::SetFlatJSObject(JSObject* object) {
  MOZ_ASSERT(!mFlatJSObject);
  MOZ_ASSERT(object);

  JS::AddAssociatedMemory(object, sizeof(*this) * GCMemoryFactor,
                          JS::MemoryUse::XPCWrappedNative);

  mFlatJSObject = object;
  mFlatJSObject.setFlags(FLAT_JS_OBJECT_VALID);
}

inline void XPCWrappedNative::UnsetFlatJSObject() {
  MOZ_ASSERT(mFlatJSObject);

  JS::RemoveAssociatedMemory(mFlatJSObject.unbarrieredGetPtr(),
                             sizeof(*this) * GCMemoryFactor,
                             JS::MemoryUse::XPCWrappedNative);

  mFlatJSObject = nullptr;
  mFlatJSObject.unsetFlags(FLAT_JS_OBJECT_VALID);
}

// This is factored out so that it can be called publicly.
// static
nsIXPCScriptable* XPCWrappedNative::GatherProtoScriptable(
    nsIClassInfo* classInfo) {
  MOZ_ASSERT(classInfo, "bad param");

  nsCOMPtr<nsIXPCScriptable> helper;
  nsresult rv = classInfo->GetScriptableHelper(getter_AddRefs(helper));
  if (NS_SUCCEEDED(rv) && helper) {
    return helper;
  }

  return nullptr;
}

// static
void XPCWrappedNative::GatherScriptable(nsISupports* aObj,
                                        nsIClassInfo* aClassInfo,
                                        nsIXPCScriptable** aScrProto,
                                        nsIXPCScriptable** aScrWrapper) {
  MOZ_ASSERT(!*aScrProto, "bad param");
  MOZ_ASSERT(!*aScrWrapper, "bad param");

  nsCOMPtr<nsIXPCScriptable> scrProto;
  nsCOMPtr<nsIXPCScriptable> scrWrapper;

  // Get the class scriptable helper (if present)
  if (aClassInfo) {
    scrProto = GatherProtoScriptable(aClassInfo);
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

    // Can't set ALLOW_PROP_MODS_DURING_RESOLVE on an instance scriptable
    // without also setting it on the class scriptable (if present).
    MOZ_ASSERT_IF(scrWrapper->AllowPropModsDuringResolve() && scrProto,
                  scrProto->AllowPropModsDuringResolve());
  } else {
    scrWrapper = scrProto;
  }

  scrProto.forget(aScrProto);
  scrWrapper.forget(aScrWrapper);
}

bool XPCWrappedNative::Init(JSContext* cx, nsIXPCScriptable* aScriptable) {
  // Setup our scriptable...
  MOZ_ASSERT(!mScriptable);
  mScriptable = aScriptable;

  // create our flatJSObject

  const JSClass* jsclazz = mScriptable ? mScriptable->GetJSClass()
                                       : Jsvalify(&XPC_WN_NoHelper_JSClass);

  // We should have the global jsclass flag if and only if we're a global.
  MOZ_ASSERT_IF(mScriptable, !!mScriptable->IsGlobalObject() ==
                                 !!(jsclazz->flags & JSCLASS_IS_GLOBAL));

  MOZ_ASSERT(jsclazz && jsclazz->name && jsclazz->flags &&
                 jsclazz->getResolve() && jsclazz->hasFinalize(),
             "bad class");

  RootedObject protoJSObject(cx, HasProto() ? GetProto()->GetJSProtoObject()
                                            : JS::GetRealmObjectPrototype(cx));
  if (!protoJSObject) {
    return false;
  }

  JSObject* object = JS_NewObjectWithGivenProto(cx, jsclazz, protoJSObject);
  if (!object) {
    return false;
  }

  SetFlatJSObject(object);

  JS_SetPrivate(mFlatJSObject, this);

  return FinishInit(cx);
}

bool XPCWrappedNative::FinishInit(JSContext* cx) {
  // This reference will be released when mFlatJSObject is finalized.
  // Since this reference will push the refcount to 2 it will also root
  // mFlatJSObject;
  MOZ_ASSERT(1 == mRefCnt, "unexpected refcount value");
  NS_ADDREF(this);

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

void XPCWrappedNative::FlatJSObjectFinalized() {
  if (!IsValid()) {
    return;
  }

  // Iterate the tearoffs and null out each of their JSObject's privates.
  // This will keep them from trying to access their pointers to the
  // dying tearoff object. We can safely assume that those remaining
  // JSObjects are about to be finalized too.

  for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to;
       to = to->GetNextTearOff()) {
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
    // As for XPCWrappedNative::Destroy, when recording or replaying the
    // release must always be deferred.
    RefPtr<nsISupports> native = to->TakeNative();
    if (native && (GetRuntime() || recordreplay::IsRecordingOrReplaying())) {
      DeferredFinalize(native.forget().take());
    }

    to->SetInterface(nullptr);
  }

  nsWrapperCache* cache = nullptr;
  CallQueryInterface(mIdentity, &cache);
  if (cache) {
    cache->ClearWrapper(mFlatJSObject.unbarrieredGetPtr());
  }

  UnsetFlatJSObject();

  MOZ_ASSERT(mIdentity, "bad pointer!");
#ifdef XP_WIN
  // Try to detect free'd pointer
  MOZ_ASSERT(*(int*)mIdentity.get() != (int)0xdddddddd, "bad pointer!");
  MOZ_ASSERT(*(int*)mIdentity.get() != (int)0, "bad pointer!");
#endif

  if (IsWrapperExpired()) {
    Destroy();
  }

  // Note that it's not safe to touch mNativeWrapper here since it's
  // likely that it has already been finalized.

  Release();
}

void XPCWrappedNative::FlatJSObjectMoved(JSObject* obj, const JSObject* old) {
  JS::AutoAssertGCCallback inCallback;
  MOZ_ASSERT(mFlatJSObject == old);

  nsWrapperCache* cache = nullptr;
  CallQueryInterface(mIdentity, &cache);
  if (cache) {
    cache->UpdateWrapper(obj, old);
  }

  mFlatJSObject = obj;
}

void XPCWrappedNative::SystemIsBeingShutDown() {
  if (!IsValid()) {
    return;
  }

  // The long standing strategy is to leak some objects still held at shutdown.
  // The general problem is that propagating release out of xpconnect at
  // shutdown time causes a world of problems.

  // We leak mIdentity (see above).

  // Short circuit future finalization.
  JS_SetPrivate(mFlatJSObject, nullptr);
  UnsetFlatJSObject();

  XPCWrappedNativeProto* proto = GetProto();

  if (HasProto()) {
    proto->SystemIsBeingShutDown();
  }

  // We don't clear mScriptable here. The destructor will do it.

  // Cleanup the tearoffs.
  for (XPCWrappedNativeTearOff* to = &mFirstTearOff; to;
       to = to->GetNextTearOff()) {
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
      : mOldReflector(cx, aOld), mNewReflector(cx, aNew) {
    MOZ_ASSERT(JS_GetPrivate(aOld) == JS_GetPrivate(aNew));
  }

  ~AutoClonePrivateGuard() {
    if (JS_GetPrivate(mOldReflector)) {
      JS_SetPrivate(mNewReflector, nullptr);
    }
  }

 private:
  RootedObject mOldReflector;
  RootedObject mNewReflector;
};

bool XPCWrappedNative::ExtendSet(JSContext* aCx,
                                 XPCNativeInterface* aInterface) {
  if (!mSet->HasInterface(aInterface)) {
    XPCNativeSetKey key(mSet, aInterface);
    RefPtr<XPCNativeSet> newSet = XPCNativeSet::GetNewOrUsed(aCx, &key);
    if (!newSet) {
      return false;
    }

    mSet = newSet.forget();
  }
  return true;
}

XPCWrappedNativeTearOff* XPCWrappedNative::FindTearOff(
    JSContext* cx, XPCNativeInterface* aInterface,
    bool needJSObject /* = false */, nsresult* pError /* = nullptr */) {
  nsresult rv = NS_OK;
  XPCWrappedNativeTearOff* to;
  XPCWrappedNativeTearOff* firstAvailable = nullptr;

  XPCWrappedNativeTearOff* lastTearOff;
  for (lastTearOff = to = &mFirstTearOff; to;
       lastTearOff = to, to = to->GetNextTearOff()) {
    if (to->GetInterface() == aInterface) {
      if (needJSObject && !to->GetJSObjectPreserveColor()) {
        AutoMarkingWrappedNativeTearOffPtr tearoff(cx, to);
        bool ok = InitTearOffJSObject(cx, to);
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
      if (pError) {
        *pError = rv;
      }
      return to;
    }
    if (!firstAvailable && to->IsAvailable()) {
      firstAvailable = to;
    }
  }

  to = firstAvailable;

  if (!to) {
    to = lastTearOff->AddTearOff();
  }

  {
    // Scope keeps |tearoff| from leaking across the rest of the function.
    AutoMarkingWrappedNativeTearOffPtr tearoff(cx, to);
    rv = InitTearOff(cx, to, aInterface, needJSObject);
    // During shutdown, we don't sweep tearoffs.  So make sure to unmark
    // manually in case the auto-marker marked us.  We shouldn't ever be
    // getting here _during_ our Mark/Sweep cycle, so this should be safe.
    to->Unmark();
    if (NS_FAILED(rv)) {
      to = nullptr;
    }
  }

  if (pError) {
    *pError = rv;
  }
  return to;
}

XPCWrappedNativeTearOff* XPCWrappedNative::FindTearOff(JSContext* cx,
                                                       const nsIID& iid) {
  RefPtr<XPCNativeInterface> iface = XPCNativeInterface::GetNewOrUsed(cx, &iid);
  return iface ? FindTearOff(cx, iface) : nullptr;
}

nsresult XPCWrappedNative::InitTearOff(JSContext* cx,
                                       XPCWrappedNativeTearOff* aTearOff,
                                       XPCNativeInterface* aInterface,
                                       bool needJSObject) {
  // Determine if the object really does this interface...

  const nsIID* iid = aInterface->GetIID();
  nsISupports* identity = GetIdentityObject();

  // This is an nsRefPtr instead of an nsCOMPtr because it may not be the
  // canonical nsISupports for this object.
  RefPtr<nsISupports> qiResult;

  // We are about to call out to other code.
  // So protect our intended tearoff.

  aTearOff->SetReserved();

  if (NS_FAILED(identity->QueryInterface(*iid, getter_AddRefs(qiResult))) ||
      !qiResult) {
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
  }

  if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateWrapper(
          cx, *iid, identity, GetClassInfo()))) {
    // the security manager vetoed. It should have set an exception.
    aTearOff->SetInterface(nullptr);
    return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
  }

  // If this is not already in our set we need to extend our set.
  // Note: we do not cache the result of the previous call to HasInterface()
  // because we unlocked and called out in the interim and the result of the
  // previous call might not be correct anymore.

  if (!mSet->HasInterface(aInterface) && !ExtendSet(cx, aInterface)) {
    aTearOff->SetInterface(nullptr);
    return NS_ERROR_NO_INTERFACE;
  }

  aTearOff->SetInterface(aInterface);
  aTearOff->SetNative(qiResult);
  RecordReplayRegisterDeferredFinalizeThing(nullptr, nullptr, qiResult);

  if (needJSObject && !InitTearOffJSObject(cx, aTearOff)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

bool XPCWrappedNative::InitTearOffJSObject(JSContext* cx,
                                           XPCWrappedNativeTearOff* to) {
  JSObject* obj = JS_NewObject(cx, Jsvalify(&XPC_WN_Tearoff_JSClass));
  if (!obj) {
    return false;
  }

  JS_SetPrivate(obj, to);
  to->SetJSObject(obj);

  js::SetReservedSlot(obj, XPC_WN_TEAROFF_FLAT_OBJECT_SLOT,
                      JS::ObjectValue(*mFlatJSObject));
  return true;
}

/***************************************************************************/

static bool Throw(nsresult errNum, XPCCallContext& ccx) {
  XPCThrower::Throw(errNum, ccx);
  return false;
}

/***************************************************************************/

class MOZ_STACK_CLASS CallMethodHelper final {
  XPCCallContext& mCallContext;
  nsresult mInvokeResult;
  const nsXPTInterfaceInfo* const mIFaceInfo;
  const nsXPTMethodInfo* mMethodInfo;
  nsISupports* const mCallee;
  const uint16_t mVTableIndex;
  HandleId mIdxValueId;

  AutoTArray<nsXPTCVariant, 8> mDispatchParams;
  uint8_t mJSContextIndex;  // TODO make const
  uint8_t mOptArgcIndex;    // TODO make const

  Value* const mArgv;
  const uint32_t mArgc;

  MOZ_ALWAYS_INLINE bool GetArraySizeFromParam(const nsXPTType& type,
                                               HandleValue maybeArray,
                                               uint32_t* result);

  MOZ_ALWAYS_INLINE bool GetInterfaceTypeFromParam(const nsXPTType& type,
                                                   nsID* result) const;

  MOZ_ALWAYS_INLINE bool GetOutParamSource(uint8_t paramIndex,
                                           MutableHandleValue srcp) const;

  MOZ_ALWAYS_INLINE bool GatherAndConvertResults();

  MOZ_ALWAYS_INLINE bool QueryInterfaceFastPath();

  nsXPTCVariant* GetDispatchParam(uint8_t paramIndex) {
    if (paramIndex >= mJSContextIndex) {
      paramIndex += 1;
    }
    if (paramIndex >= mOptArgcIndex) {
      paramIndex += 1;
    }
    return &mDispatchParams[paramIndex];
  }
  const nsXPTCVariant* GetDispatchParam(uint8_t paramIndex) const {
    return const_cast<CallMethodHelper*>(this)->GetDispatchParam(paramIndex);
  }

  MOZ_ALWAYS_INLINE bool InitializeDispatchParams();

  MOZ_ALWAYS_INLINE bool ConvertIndependentParams(bool* foundDependentParam);
  MOZ_ALWAYS_INLINE bool ConvertIndependentParam(uint8_t i);
  MOZ_ALWAYS_INLINE bool ConvertDependentParams();
  MOZ_ALWAYS_INLINE bool ConvertDependentParam(uint8_t i);

  MOZ_ALWAYS_INLINE nsresult Invoke();

 public:
  explicit CallMethodHelper(XPCCallContext& ccx)
      : mCallContext(ccx),
        mInvokeResult(NS_ERROR_UNEXPECTED),
        mIFaceInfo(ccx.GetInterface()->GetInterfaceInfo()),
        mMethodInfo(nullptr),
        mCallee(ccx.GetTearOff()->GetNative()),
        mVTableIndex(ccx.GetMethodIndex()),
        mIdxValueId(ccx.GetContext()->GetStringID(XPCJSContext::IDX_VALUE)),
        mJSContextIndex(UINT8_MAX),
        mOptArgcIndex(UINT8_MAX),
        mArgv(ccx.GetArgv()),
        mArgc(ccx.GetArgc())

  {
    // Success checked later.
    mIFaceInfo->GetMethodInfo(mVTableIndex, &mMethodInfo);
  }

  ~CallMethodHelper();

  MOZ_ALWAYS_INLINE bool Call();

  // Trace implementation so we can put our CallMethodHelper in a Rooted<T>.
  void trace(JSTracer* aTrc);
};

// static
bool XPCWrappedNative::CallMethod(XPCCallContext& ccx,
                                  CallMode mode /*= CALL_METHOD */) {
  nsresult rv = ccx.CanCallNow();
  if (NS_FAILED(rv)) {
    return Throw(rv, ccx);
  }

  JS::Rooted<CallMethodHelper> helper(ccx, /* init = */ ccx);
  return helper.get().Call();
}

bool CallMethodHelper::Call() {
  mCallContext.SetRetVal(JS::UndefinedValue());

  mCallContext.GetContext()->SetPendingException(nullptr);

  if (mVTableIndex == 0) {
    return QueryInterfaceFastPath();
  }

  if (!mMethodInfo) {
    Throw(NS_ERROR_XPC_CANT_GET_METHOD_INFO, mCallContext);
    return false;
  }

  if (!InitializeDispatchParams()) {
    return false;
  }

  // Iterate through the params doing conversions of independent params only.
  // When we later convert the dependent params (if any) we will know that
  // the params upon which they depend will have already been converted -
  // regardless of ordering.
  bool foundDependentParam = false;
  if (!ConvertIndependentParams(&foundDependentParam)) {
    return false;
  }

  if (foundDependentParam && !ConvertDependentParams()) {
    return false;
  }

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

CallMethodHelper::~CallMethodHelper() {
  for (nsXPTCVariant& param : mDispatchParams) {
    uint32_t arraylen = 0;
    if (!GetArraySizeFromParam(param.type, UndefinedHandleValue, &arraylen)) {
      continue;
    }

    xpc::DestructValue(param.type, &param.val, arraylen);
  }
}

bool CallMethodHelper::GetArraySizeFromParam(const nsXPTType& type,
                                             HandleValue maybeArray,
                                             uint32_t* result) {
  if (type.Tag() != nsXPTType::T_LEGACY_ARRAY &&
      type.Tag() != nsXPTType::T_PSTRING_SIZE_IS &&
      type.Tag() != nsXPTType::T_PWSTRING_SIZE_IS) {
    *result = 0;
    return true;
  }

  uint8_t argnum = type.ArgNum();
  uint32_t* lengthp = &GetDispatchParam(argnum)->val.u32;

  // TODO fixup the various exceptions that are thrown

  // If the array length wasn't passed, it might have been listed as optional.
  // When converting arguments from JS to C++, we pass the array as
  // |maybeArray|, and give ourselves the chance to infer the length. Once we
  // have it, we stick it in the right slot so that we can find it again when
  // cleaning up the params. from the array.
  if (argnum >= mArgc && maybeArray.isObject()) {
    MOZ_ASSERT(mMethodInfo->Param(argnum).IsOptional());
    RootedObject arrayOrNull(mCallContext, &maybeArray.toObject());

    bool isArray;
    bool ok = false;
    if (JS_IsArrayObject(mCallContext, maybeArray, &isArray) && isArray) {
      ok = JS_GetArrayLength(mCallContext, arrayOrNull, lengthp);
    } else if (JS_IsTypedArrayObject(&maybeArray.toObject())) {
      *lengthp = JS_GetTypedArrayLength(&maybeArray.toObject());
      ok = true;
    }

    if (!ok) {
      return Throw(NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY, mCallContext);
    }
  }

  *result = *lengthp;
  return true;
}

bool CallMethodHelper::GetInterfaceTypeFromParam(const nsXPTType& type,
                                                 nsID* result) const {
  result->Clear();

  const nsXPTType& inner = type.InnermostType();
  if (inner.Tag() == nsXPTType::T_INTERFACE) {
    if (!inner.GetInterface()) {
      return Throw(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO, mCallContext);
    }

    *result = inner.GetInterface()->IID();
  } else if (inner.Tag() == nsXPTType::T_INTERFACE_IS) {
    const nsXPTCVariant* param = GetDispatchParam(inner.ArgNum());
    if (param->type.Tag() != nsXPTType::T_NSID &&
        param->type.Tag() != nsXPTType::T_NSIDPTR) {
      return Throw(NS_ERROR_UNEXPECTED, mCallContext);
    }

    const void* ptr = &param->val;
    if (param->type.Tag() == nsXPTType::T_NSIDPTR) {
      ptr = *static_cast<nsID* const*>(ptr);
    }

    if (!ptr) {
      return ThrowBadParam(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,
                           inner.ArgNum(), mCallContext);
    }

    *result = *static_cast<const nsID*>(ptr);
  }
  return true;
}

bool CallMethodHelper::GetOutParamSource(uint8_t paramIndex,
                                         MutableHandleValue srcp) const {
  const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(paramIndex);
  bool isRetval = &paramInfo == mMethodInfo->GetRetval();

  if (paramInfo.IsOut() && !isRetval) {
    MOZ_ASSERT(paramIndex < mArgc || paramInfo.IsOptional(),
               "Expected either enough arguments or an optional argument");
    Value arg = paramIndex < mArgc ? mArgv[paramIndex] : JS::NullValue();
    if (paramIndex < mArgc) {
      RootedObject obj(mCallContext);
      if (!arg.isPrimitive()) {
        obj = &arg.toObject();
      }
      if (!obj || !JS_GetPropertyById(mCallContext, obj, mIdxValueId, srcp)) {
        // Explicitly passed in unusable value for out param.  Note
        // that if i >= mArgc we already know that |arg| is JS::NullValue(),
        // and that's ok.
        ThrowBadParam(NS_ERROR_XPC_NEED_OUT_OBJECT, paramIndex, mCallContext);
        return false;
      }
    }
  }

  return true;
}

bool CallMethodHelper::GatherAndConvertResults() {
  // now we iterate through the native params to gather and convert results
  uint8_t paramCount = mMethodInfo->GetParamCount();
  for (uint8_t i = 0; i < paramCount; i++) {
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
    if (!paramInfo.IsOut()) {
      continue;
    }

    const nsXPTType& type = paramInfo.GetType();
    nsXPTCVariant* dp = GetDispatchParam(i);
    RootedValue v(mCallContext, NullValue());

    uint32_t array_count = 0;
    nsID param_iid;
    if (!GetInterfaceTypeFromParam(type, &param_iid) ||
        !GetArraySizeFromParam(type, UndefinedHandleValue, &array_count))
      return false;

    nsresult err;
    if (!XPCConvert::NativeData2JS(mCallContext, &v, &dp->val, type, &param_iid,
                                   array_count, &err)) {
      ThrowBadParam(err, i, mCallContext);
      return false;
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

bool CallMethodHelper::QueryInterfaceFastPath() {
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

  JS::RootedValue iidarg(mCallContext, mArgv[0]);
  Maybe<nsID> iid = xpc::JSValue2ID(mCallContext, iidarg);
  if (!iid) {
    ThrowBadParam(NS_ERROR_XPC_BAD_CONVERT_JS, 0, mCallContext);
    return false;
  }

  nsISupports* qiresult = nullptr;
  mInvokeResult = mCallee->QueryInterface(iid.ref(), (void**)&qiresult);

  if (NS_FAILED(mInvokeResult)) {
    ThrowBadResult(mInvokeResult, mCallContext);
    return false;
  }

  RootedValue v(mCallContext, NullValue());
  nsresult err;
  bool success = XPCConvert::NativeData2JS(mCallContext, &v, &qiresult,
                                           {nsXPTType::T_INTERFACE_IS},
                                           iid.ptr(), 0, &err);
  NS_IF_RELEASE(qiresult);

  if (!success) {
    ThrowBadParam(err, 0, mCallContext);
    return false;
  }

  mCallContext.SetRetVal(v);
  return true;
}

bool CallMethodHelper::InitializeDispatchParams() {
  const uint8_t wantsOptArgc = mMethodInfo->WantsOptArgc() ? 1 : 0;
  const uint8_t wantsJSContext = mMethodInfo->WantsContext() ? 1 : 0;
  const uint8_t paramCount = mMethodInfo->GetParamCount();
  uint8_t requiredArgs = paramCount;

  // XXX ASSUMES that retval is last arg. The xpidl compiler ensures this.
  if (mMethodInfo->HasRetval()) {
    requiredArgs--;
  }

  if (mArgc < requiredArgs || wantsOptArgc) {
    if (wantsOptArgc) {
      // The implicit JSContext*, if we have one, comes first.
      mOptArgcIndex = requiredArgs + wantsJSContext;
    }

    // skip over any optional arguments
    while (requiredArgs &&
           mMethodInfo->GetParam(requiredArgs - 1).IsOptional()) {
      requiredArgs--;
    }

    if (mArgc < requiredArgs) {
      Throw(NS_ERROR_XPC_NOT_ENOUGH_ARGS, mCallContext);
      return false;
    }
  }

  mJSContextIndex = mMethodInfo->IndexOfJSContext();

  // Allocate enough space in mDispatchParams up-front.
  if (!mDispatchParams.AppendElements(paramCount + wantsJSContext +
                                      wantsOptArgc)) {
    Throw(NS_ERROR_OUT_OF_MEMORY, mCallContext);
    return false;
  }

  // Initialize each parameter to a valid state (for safe cleanup later).
  for (uint8_t i = 0, paramIdx = 0; i < mDispatchParams.Length(); i++) {
    nsXPTCVariant& dp = mDispatchParams[i];

    if (i == mJSContextIndex) {
      // Fill in the JSContext argument
      dp.type = nsXPTType::T_VOID;
      dp.val.p = mCallContext;
    } else if (i == mOptArgcIndex) {
      // Fill in the optional_argc argument
      dp.type = nsXPTType::T_U8;
      dp.val.u8 = std::min<uint32_t>(mArgc, paramCount) - requiredArgs;
    } else {
      // Initialize normal arguments.
      const nsXPTParamInfo& param = mMethodInfo->Param(paramIdx);
      dp.type = param.Type();
      xpc::InitializeValue(dp.type, &dp.val);

      // Specify the correct storage/calling semantics. This will also set
      // the `ptr` field to be self-referential.
      if (param.IsIndirect()) {
        dp.SetIndirect();
      }

      // Advance to the next normal parameter.
      paramIdx++;
    }
  }

  return true;
}

bool CallMethodHelper::ConvertIndependentParams(bool* foundDependentParam) {
  const uint8_t paramCount = mMethodInfo->GetParamCount();
  for (uint8_t i = 0; i < paramCount; i++) {
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);

    if (paramInfo.GetType().IsDependent()) {
      *foundDependentParam = true;
    } else if (!ConvertIndependentParam(i)) {
      return false;
    }
  }

  return true;
}

bool CallMethodHelper::ConvertIndependentParam(uint8_t i) {
  const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
  const nsXPTType& type = paramInfo.Type();
  nsXPTCVariant* dp = GetDispatchParam(i);

  // Even if there's nothing to convert, we still need to examine the
  // JSObject container for out-params. If it's null or otherwise invalid,
  // we want to know before the call, rather than after.
  //
  // This is a no-op for 'in' params.
  RootedValue src(mCallContext);
  if (!GetOutParamSource(i, &src)) {
    return false;
  }

  // All that's left to do is value conversion. Bail early if we don't need
  // to do that.
  if (!paramInfo.IsIn()) {
    return true;
  }

  // Some types usually don't support default values, but we want to handle
  // the default value if IsOptional is true.
  if (i >= mArgc) {
    MOZ_ASSERT(paramInfo.IsOptional(), "missing non-optional argument!");
    if (type.Tag() == nsXPTType::T_NSID) {
      // Use a default value of the null ID for optional NSID objects.
      dp->ext.nsid.Clear();
      return true;
    }

    if (type.Tag() == nsXPTType::T_ARRAY) {
      // Use a default value of empty array for optional Array objects.
      dp->ext.array.Clear();
      return true;
    }
  }

  // We're definitely some variety of 'in' now, so there's something to
  // convert. The source value for conversion depends on whether we're
  // dealing with an 'in' or an 'inout' parameter. 'inout' was handled above,
  // so all that's left is 'in'.
  if (!paramInfo.IsOut()) {
    // Handle the 'in' case.
    MOZ_ASSERT(i < mArgc || paramInfo.IsOptional(),
               "Expected either enough arguments or an optional argument");
    if (i < mArgc) {
      src = mArgv[i];
    } else if (type.Tag() == nsXPTType::T_JSVAL) {
      src.setUndefined();
    } else {
      src.setNull();
    }
  }

  nsID param_iid = {0};
  const nsXPTType& inner = type.InnermostType();
  if (inner.Tag() == nsXPTType::T_INTERFACE) {
    if (!inner.GetInterface()) {
      return ThrowBadParam(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO, i,
                           mCallContext);
    }
    param_iid = inner.GetInterface()->IID();
  }

  // Don't allow CPOWs to be passed to native code (in case they try to cast
  // to a concrete type).
  if (src.isObject() && jsipc::IsWrappedCPOW(&src.toObject()) &&
      type.Tag() == nsXPTType::T_INTERFACE &&
      !param_iid.Equals(NS_GET_IID(nsISupports))) {
    // Allow passing CPOWs to XPCWrappedJS.
    nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(mCallee));
    if (!wrappedJS) {
      ThrowBadParam(NS_ERROR_XPC_CANT_PASS_CPOW_TO_NATIVE, i, mCallContext);
      return false;
    }
  }

  nsresult err;
  if (!XPCConvert::JSData2Native(mCallContext, &dp->val, src, type, &param_iid,
                                 0, &err)) {
    ThrowBadParam(err, i, mCallContext);
    return false;
  }

  return true;
}

bool CallMethodHelper::ConvertDependentParams() {
  const uint8_t paramCount = mMethodInfo->GetParamCount();
  for (uint8_t i = 0; i < paramCount; i++) {
    const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);

    if (!paramInfo.GetType().IsDependent()) {
      continue;
    }
    if (!ConvertDependentParam(i)) {
      return false;
    }
  }

  return true;
}

bool CallMethodHelper::ConvertDependentParam(uint8_t i) {
  const nsXPTParamInfo& paramInfo = mMethodInfo->GetParam(i);
  const nsXPTType& type = paramInfo.Type();
  nsXPTCVariant* dp = GetDispatchParam(i);

  // Even if there's nothing to convert, we still need to examine the
  // JSObject container for out-params. If it's null or otherwise invalid,
  // we want to know before the call, rather than after.
  //
  // This is a no-op for 'in' params.
  RootedValue src(mCallContext);
  if (!GetOutParamSource(i, &src)) {
    return false;
  }

  // All that's left to do is value conversion. Bail early if we don't need
  // to do that.
  if (!paramInfo.IsIn()) {
    return true;
  }

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
  uint32_t array_count;
  if (!GetInterfaceTypeFromParam(type, &param_iid) ||
      !GetArraySizeFromParam(type, src, &array_count))
    return false;

  nsresult err;

  if (!XPCConvert::JSData2Native(mCallContext, &dp->val, src, type, &param_iid,
                                 array_count, &err)) {
    ThrowBadParam(err, i, mCallContext);
    return false;
  }

  return true;
}

nsresult CallMethodHelper::Invoke() {
  uint32_t argc = mDispatchParams.Length();
  nsXPTCVariant* argv = mDispatchParams.Elements();

  return NS_InvokeByIndex(mCallee, mVTableIndex, argc, argv);
}

static void TraceParam(JSTracer* aTrc, void* aVal, const nsXPTType& aType,
                       uint32_t aArrayLen = 0) {
  if (aType.Tag() == nsXPTType::T_JSVAL) {
    JS::UnsafeTraceRoot(aTrc, (JS::Value*)aVal,
                        "XPCWrappedNative::CallMethod param");
  } else if (aType.Tag() == nsXPTType::T_ARRAY) {
    auto* array = (xpt::detail::UntypedTArray*)aVal;
    const nsXPTType& elty = aType.ArrayElementType();

    for (uint32_t i = 0; i < array->Length(); ++i) {
      TraceParam(aTrc, elty.ElementPtr(array->Elements(), i), elty);
    }
  } else if (aType.Tag() == nsXPTType::T_LEGACY_ARRAY && *(void**)aVal) {
    const nsXPTType& elty = aType.ArrayElementType();

    for (uint32_t i = 0; i < aArrayLen; ++i) {
      TraceParam(aTrc, elty.ElementPtr(*(void**)aVal, i), elty);
    }
  }
}

void CallMethodHelper::trace(JSTracer* aTrc) {
  // We need to note each of our initialized parameters which contain jsvals.
  for (nsXPTCVariant& param : mDispatchParams) {
    // We only need to trace parameters which have an innermost JSVAL.
    if (param.type.InnermostType().Tag() != nsXPTType::T_JSVAL) {
      continue;
    }

    uint32_t arrayLen = 0;
    if (!GetArraySizeFromParam(param.type, UndefinedHandleValue, &arrayLen)) {
      continue;
    }

    TraceParam(aTrc, &param.val, param.type, arrayLen);
  }
}

/***************************************************************************/
// interface methods

JSObject* XPCWrappedNative::GetJSObject() { return GetFlatJSObject(); }

NS_IMETHODIMP XPCWrappedNative::DebugDump(int16_t depth) {
#ifdef DEBUG
  depth--;
  XPC_LOG_ALWAYS(
      ("XPCWrappedNative @ %p with mRefCnt = %" PRIuPTR, this, mRefCnt.get()));
  XPC_LOG_INDENT();

  if (HasProto()) {
    XPCWrappedNativeProto* proto = GetProto();
    if (depth && proto) {
      proto->DebugDump(depth);
    } else {
      XPC_LOG_ALWAYS(("mMaybeProto @ %p", proto));
    }
  } else
    XPC_LOG_ALWAYS(("Scope @ %p", GetScope()));

  if (depth && mSet) {
    mSet->DebugDump(depth);
  } else {
    XPC_LOG_ALWAYS(("mSet @ %p", mSet.get()));
  }

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

char* XPCWrappedNative::ToString(
    JSContext* cx, XPCWrappedNativeTearOff* to /* = nullptr */) const {
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
  if (scr) {
    name = JS_smprintf("%s", scr->GetJSClass()->name);
  }
  if (to) {
    const char* fmt = name ? " (%s)" : "%s";
    name = JS_sprintf_append(std::move(name), fmt,
                             to->GetInterface()->GetNameString());
  } else if (!name) {
    XPCNativeSet* set = GetSet();
    XPCNativeInterface** array = set->GetInterfaceArray();
    RefPtr<XPCNativeInterface> isupp = XPCNativeInterface::GetISupports(cx);
    uint16_t count = set->GetInterfaceCount();

    if (count == 1) {
      name =
          JS_sprintf_append(std::move(name), "%s", array[0]->GetNameString());
    } else if (count == 2 && array[0] == isupp) {
      name =
          JS_sprintf_append(std::move(name), "%s", array[1]->GetNameString());
    } else {
      for (uint16_t i = 0; i < count; i++) {
        const char* fmt =
            (i == 0) ? "(%s" : (i == count - 1) ? ", %s)" : ", %s";
        name =
            JS_sprintf_append(std::move(name), fmt, array[i]->GetNameString());
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
  sz =
      JS_smprintf(fmt, name.get() PARAM_ADDR(this) PARAM_ADDR(mIdentity.get()));

  return sz.release();

#undef FMT_ADDR
#undef PARAM_ADDR
}

/***************************************************************************/

#ifdef XPC_CHECK_CLASSINFO_CLAIMS
static void DEBUG_CheckClassInfoClaims(XPCWrappedNative* wrapper) {
  if (!wrapper || !wrapper->GetClassInfo()) {
    return;
  }

  nsISupports* obj = wrapper->GetIdentityObject();
  XPCNativeSet* set = wrapper->GetSet();
  uint16_t count = set->GetInterfaceCount();
  for (uint16_t i = 0; i < count; i++) {
    nsIClassInfo* clsInfo = wrapper->GetClassInfo();
    XPCNativeInterface* iface = set->GetInterfaceAt(i);
    const nsXPTInterfaceInfo* info = iface->GetInterfaceInfo();
    nsISupports* ptr;

    nsresult rv = obj->QueryInterface(info->IID(), (void**)&ptr);
    if (NS_SUCCEEDED(rv)) {
      NS_RELEASE(ptr);
      continue;
    }
    if (rv == NS_ERROR_OUT_OF_MEMORY) {
      continue;
    }

    // Houston, We have a problem...

    char* className = nullptr;
    char* contractID = nullptr;
    const char* interfaceName = info->Name();

    clsInfo->GetContractID(&contractID);
    if (wrapper->GetScriptable()) {
      wrapper->GetScriptable()->GetClassName(&className);
    }

    printf(
        "\n!!! Object's nsIClassInfo lies about its interfaces!!!\n"
        "   classname: %s \n"
        "   contractid: %s \n"
        "   unimplemented interface name: %s\n\n",
        className ? className : "<unknown>",
        contractID ? contractID : "<unknown>", interfaceName);

    if (className) {
      free(className);
    }
    if (contractID) {
      free(contractID);
    }
  }
}
#endif
