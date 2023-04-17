/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class used to manage the wrapped native objects within a JS scope. */

#include "AccessCheck.h"
#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "ExpandedPrincipal.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "js/Object.h"              // JS::GetCompartment
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefinePropertyById
#include "js/RealmIterators.h"
#include "mozJSModuleLoader.h"

#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace xpc;
using namespace JS;

/***************************************************************************/

static XPCWrappedNativeScopeList& AllScopes() {
  return XPCJSRuntime::Get()->GetWrappedNativeScopes();
}

static bool RemoteXULForbidsXBLScopeForPrincipal(nsIPrincipal* aPrincipal) {
  // AllowXULXBLForPrincipal will return true for system principal, but we
  // don't want that here.
  MOZ_ASSERT(nsContentUtils::IsInitialized());
  if (aPrincipal->IsSystemPrincipal()) {
    return false;
  }

  // If this domain isn't whitelisted, we're done.
  if (!nsContentUtils::AllowXULXBLForPrincipal(aPrincipal)) {
    return false;
  }

  // Check the pref to determine how we should behave.
  return !Preferences::GetBool("dom.use_xbl_scopes_for_remote_xul", false);
}

static bool RemoteXULForbidsXBLScope(HandleObject aFirstGlobal) {
  MOZ_ASSERT(aFirstGlobal);

  // Certain singleton sandoxes are created very early in startup - too early
  // to call into AllowXULXBLForPrincipal. We never create XBL scopes for
  // sandboxes anway, and certainly not for these singleton scopes. So we just
  // short-circuit here.
  if (IsSandbox(aFirstGlobal)) {
    return false;
  }

  nsIPrincipal* principal = xpc::GetObjectPrincipal(aFirstGlobal);
  return RemoteXULForbidsXBLScopeForPrincipal(principal);
}

XPCWrappedNativeScope::XPCWrappedNativeScope(JS::Compartment* aCompartment,
                                             JS::HandleObject aFirstGlobal)
    : mWrappedNativeMap(mozilla::MakeUnique<Native2WrappedNativeMap>()),
      mWrappedNativeProtoMap(
          mozilla::MakeUnique<ClassInfo2WrappedNativeProtoMap>()),
      mComponents(nullptr),
      mCompartment(aCompartment) {
#ifdef DEBUG
  for (XPCWrappedNativeScope* cur : AllScopes()) {
    MOZ_ASSERT(aCompartment != cur->Compartment(), "dup object");
  }
#endif

  AllScopes().insertBack(this);

  MOZ_COUNT_CTOR(XPCWrappedNativeScope);

  // Determine whether we would allow an XBL scope in this situation.
  // In addition to being pref-controlled, we also disable XBL scopes for
  // remote XUL domains, _except_ if we have an additional pref override set.
  //
  // Note that we can't quite remove this yet, even though we never actually
  // use XBL scopes, because the security manager uses this boolean to make
  // decisions that we rely on in our test infrastructure.
  //
  // FIXME(emilio): Now that the security manager is the only caller probably
  // should be renamed, but what's a good name for this?
  mAllowContentXBLScope = !RemoteXULForbidsXBLScope(aFirstGlobal);
}

bool XPCWrappedNativeScope::GetComponentsJSObject(JSContext* cx,
                                                  JS::MutableHandleObject obj) {
  if (!mComponents) {
    bool system = AccessCheck::isChrome(mCompartment);
    MOZ_RELEASE_ASSERT(system, "How did we get a non-system Components?");
    mComponents = new nsXPCComponents(this);
  }

  RootedValue val(cx);
  xpcObjectHelper helper(mComponents);
  bool ok = XPCConvert::NativeInterface2JSObject(cx, &val, helper, nullptr,
                                                 false, nullptr);
  if (NS_WARN_IF(!ok)) {
    return false;
  }

  if (NS_WARN_IF(!val.isObject())) {
    return false;
  }

  obj.set(&val.toObject());
  return true;
}

static bool DefineSubcomponentProperty(JSContext* aCx, HandleObject aGlobal,
                                       nsISupports* aSubcomponent,
                                       const nsID* aIID,
                                       unsigned int aStringIndex) {
  RootedValue subcompVal(aCx);
  xpcObjectHelper helper(aSubcomponent);
  if (!XPCConvert::NativeInterface2JSObject(aCx, &subcompVal, helper, aIID,
                                            false, nullptr))
    return false;
  if (NS_WARN_IF(!subcompVal.isObject())) {
    return false;
  }
  RootedId id(aCx, XPCJSContext::Get()->GetStringID(aStringIndex));
  return JS_DefinePropertyById(aCx, aGlobal, id, subcompVal, 0);
}

bool XPCWrappedNativeScope::AttachComponentsObject(JSContext* aCx) {
  RootedObject components(aCx);
  if (!GetComponentsJSObject(aCx, &components)) {
    return false;
  }

  RootedObject global(aCx, CurrentGlobalOrNull(aCx));

  const unsigned attrs = JSPROP_READONLY | JSPROP_RESOLVING | JSPROP_PERMANENT;

  RootedId id(aCx,
              XPCJSContext::Get()->GetStringID(XPCJSContext::IDX_COMPONENTS));
  if (!JS_DefinePropertyById(aCx, global, id, components, attrs)) {
    return false;
  }

// _iid can be nullptr if the object implements classinfo.
#define DEFINE_SUBCOMPONENT_PROPERTY(_comp, _type, _iid, _id)                 \
  nsCOMPtr<nsIXPCComponents_##_type> obj##_type;                              \
  if (NS_FAILED(_comp->Get##_type(getter_AddRefs(obj##_type)))) return false; \
  if (!DefineSubcomponentProperty(aCx, global, obj##_type, _iid,              \
                                  XPCJSContext::IDX_##_id))                   \
    return false;

  DEFINE_SUBCOMPONENT_PROPERTY(mComponents, Interfaces, nullptr, CI)
  DEFINE_SUBCOMPONENT_PROPERTY(mComponents, Results, nullptr, CR)

  DEFINE_SUBCOMPONENT_PROPERTY(mComponents, Classes, nullptr, CC)
  DEFINE_SUBCOMPONENT_PROPERTY(mComponents, Utils,
                               &NS_GET_IID(nsIXPCComponents_Utils), CU)

#undef DEFINE_SUBCOMPONENT_PROPERTY

  return true;
}

bool XPCWrappedNativeScope::AttachJSServices(JSContext* aCx) {
  RootedObject global(aCx, CurrentGlobalOrNull(aCx));
  return mozJSModuleLoader::Get()->DefineJSServices(aCx, global);
}

bool XPCWrappedNativeScope::XBLScopeStateMatches(nsIPrincipal* aPrincipal) {
  return mAllowContentXBLScope ==
         !RemoteXULForbidsXBLScopeForPrincipal(aPrincipal);
}

bool XPCWrappedNativeScope::AllowContentXBLScope(Realm* aRealm) {
  // We only disallow XBL scopes in remote XUL situations.
  MOZ_ASSERT_IF(!mAllowContentXBLScope, nsContentUtils::AllowXULXBLForPrincipal(
                                            xpc::GetRealmPrincipal(aRealm)));
  return mAllowContentXBLScope;
}

namespace xpc {
JSObject* GetUAWidgetScope(JSContext* cx, JSObject* contentScopeArg) {
  JS::RootedObject contentScope(cx, contentScopeArg);
  JSAutoRealm ar(cx, contentScope);
  nsIPrincipal* principal = GetObjectPrincipal(contentScope);

  if (principal->IsSystemPrincipal()) {
    return JS::GetNonCCWObjectGlobal(contentScope);
  }

  return GetUAWidgetScope(cx, principal);
}

JSObject* GetUAWidgetScope(JSContext* cx, nsIPrincipal* principal) {
  RootedObject scope(cx, XPCJSRuntime::Get()->GetUAWidgetScope(cx, principal));
  NS_ENSURE_TRUE(scope, nullptr);  // See bug 858642.

  scope = js::UncheckedUnwrap(scope);
  JS::ExposeObjectToActiveJS(scope);
  return scope;
}

bool AllowContentXBLScope(JS::Realm* realm) {
  JS::Compartment* comp = GetCompartmentForRealm(realm);
  XPCWrappedNativeScope* scope = CompartmentPrivate::Get(comp)->GetScope();
  MOZ_ASSERT(scope);
  return scope->AllowContentXBLScope(realm);
}

} /* namespace xpc */

XPCWrappedNativeScope::~XPCWrappedNativeScope() {
  MOZ_COUNT_DTOR(XPCWrappedNativeScope);

  // We can do additional cleanup assertions here...

  MOZ_ASSERT(0 == mWrappedNativeMap->Count(), "scope has non-empty map");

  MOZ_ASSERT(0 == mWrappedNativeProtoMap->Count(), "scope has non-empty map");

  // This should not be necessary, since the Components object should die
  // with the scope but just in case.
  if (mComponents) {
    mComponents->mScope = nullptr;
  }

  // XXX we should assert that we are dead or that xpconnect has shutdown
  // XXX might not want to do this at xpconnect shutdown time???
  mComponents = nullptr;

  MOZ_RELEASE_ASSERT(!mXrayExpandos.initialized());

  mCompartment = nullptr;
}

// static
void XPCWrappedNativeScope::TraceWrappedNativesInAllScopes(XPCJSRuntime* xpcrt,
                                                           JSTracer* trc) {
  // Do JS::TraceEdge for all wrapped natives with external references, as
  // well as any DOM expando objects.
  //
  // Note: the GC can call this from a JS helper thread. We don't use
  // AllScopes() because that asserts we're on the main thread.

  for (XPCWrappedNativeScope* cur : xpcrt->GetWrappedNativeScopes()) {
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.done(); i.next()) {
      XPCWrappedNative* wrapper = i.get().value();
      if (wrapper->HasExternalReference() && !wrapper->IsWrapperExpired()) {
        wrapper->TraceSelf(trc);
      }
    }
  }
}

// static
void XPCWrappedNativeScope::SuspectAllWrappers(
    nsCycleCollectionNoteRootCallback& cb) {
  for (XPCWrappedNativeScope* cur : AllScopes()) {
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.done(); i.next()) {
      i.get().value()->Suspect(cb);
    }
  }
}

void XPCWrappedNativeScope::UpdateWeakPointersAfterGC(JSTracer* trc) {
  // Sweep waivers.
  if (mWaiverWrapperMap) {
    mWaiverWrapperMap->UpdateWeakPointers(trc);
  }

  if (!js::IsCompartmentZoneSweepingOrCompacting(mCompartment)) {
    return;
  }

  if (!js::CompartmentHasLiveGlobal(mCompartment)) {
    GetWrappedNativeMap()->Clear();
    mWrappedNativeProtoMap->Clear();

    // The fields below are traced only if there's a live global in the
    // compartment, see TraceXPCGlobal. The compartment has no live globals so
    // clear these pointers here.
    if (mXrayExpandos.initialized()) {
      mXrayExpandos.destroy();
    }
    mIDProto = nullptr;
    mIIDProto = nullptr;
    mCIDProto = nullptr;
    return;
  }

  // Sweep mWrappedNativeMap for dying flat JS objects. Moving has already
  // been handled by XPCWrappedNative::FlatJSObjectMoved.
  for (auto iter = GetWrappedNativeMap()->ModIter(); !iter.done();
       iter.next()) {
    XPCWrappedNative* wrapper = iter.get().value();
    JSObject* obj = wrapper->GetFlatJSObjectPreserveColor();
    if (JS_UpdateWeakPointerAfterGCUnbarriered(trc, &obj)) {
      MOZ_ASSERT(obj == wrapper->GetFlatJSObjectPreserveColor());
      MOZ_ASSERT(JS::GetCompartment(obj) == mCompartment);
    } else {
      iter.remove();
    }
  }

  // Sweep mWrappedNativeProtoMap for dying prototype JSObjects. Moving has
  // already been handled by XPCWrappedNativeProto::JSProtoObjectMoved.
  for (auto i = mWrappedNativeProtoMap->ModIter(); !i.done(); i.next()) {
    XPCWrappedNativeProto* proto = i.get().value();
    JSObject* obj = proto->GetJSProtoObjectPreserveColor();
    if (JS_UpdateWeakPointerAfterGCUnbarriered(trc, &obj)) {
      MOZ_ASSERT(JS::GetCompartment(obj) == mCompartment);
      MOZ_ASSERT(obj == proto->GetJSProtoObjectPreserveColor());
    } else {
      i.remove();
    }
  }
}

// static
void XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs() {
  for (XPCWrappedNativeScope* cur : AllScopes()) {
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.done(); i.next()) {
      i.get().value()->SweepTearOffs();
    }
  }
}

// static
void XPCWrappedNativeScope::SystemIsBeingShutDown() {
  // We're forcibly killing scopes, rather than allowing them to go away
  // when they're ready. As such, we need to do some cleanup before they
  // can safely be destroyed.

  for (XPCWrappedNativeScope* cur : AllScopes()) {
    // Give the Components object a chance to try to clean up.
    if (cur->mComponents) {
      cur->mComponents->SystemIsBeingShutDown();
    }

    // Null out these pointers to prevent ~ObjectPtr assertion failures if we
    // leaked things at shutdown.
    cur->mIDProto = nullptr;
    cur->mIIDProto = nullptr;
    cur->mCIDProto = nullptr;

    // Similarly, destroy mXrayExpandos to prevent assertion failures.
    if (cur->mXrayExpandos.initialized()) {
      cur->mXrayExpandos.destroy();
    }

    // Walk the protos first. Wrapper shutdown can leave dangling
    // proto pointers in the proto map.
    for (auto i = cur->mWrappedNativeProtoMap->ModIter(); !i.done(); i.next()) {
      i.get().value()->SystemIsBeingShutDown();
      i.remove();
    }
    for (auto i = cur->mWrappedNativeMap->ModIter(); !i.done(); i.next()) {
      i.get().value()->SystemIsBeingShutDown();
      i.remove();
    }

    CompartmentPrivate* priv = CompartmentPrivate::Get(cur->Compartment());
    priv->SystemIsBeingShutDown();
  }
}

/***************************************************************************/

JSObject* XPCWrappedNativeScope::GetExpandoChain(HandleObject target) {
  MOZ_ASSERT(ObjectScope(target) == this);
  if (!mXrayExpandos.initialized()) {
    return nullptr;
  }
  return mXrayExpandos.lookup(target);
}

JSObject* XPCWrappedNativeScope::DetachExpandoChain(HandleObject target) {
  MOZ_ASSERT(ObjectScope(target) == this);
  if (!mXrayExpandos.initialized()) {
    return nullptr;
  }
  return mXrayExpandos.removeValue(target);
}

bool XPCWrappedNativeScope::SetExpandoChain(JSContext* cx, HandleObject target,
                                            HandleObject chain) {
  MOZ_ASSERT(ObjectScope(target) == this);
  MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
  MOZ_ASSERT_IF(chain, ObjectScope(chain) == this);
  if (!mXrayExpandos.initialized() && !mXrayExpandos.init(cx)) {
    return false;
  }
  return mXrayExpandos.put(cx, target, chain);
}

/***************************************************************************/

// static
void XPCWrappedNativeScope::DebugDumpAllScopes(int16_t depth) {
#ifdef DEBUG
  depth--;

  // get scope count.
  int count = 0;
  for (XPCWrappedNativeScope* cur : AllScopes()) {
    mozilla::Unused << cur;
    count++;
  }

  XPC_LOG_ALWAYS(("chain of %d XPCWrappedNativeScope(s)", count));
  XPC_LOG_INDENT();
  if (depth) {
    for (XPCWrappedNativeScope* cur : AllScopes()) {
      cur->DebugDump(depth);
    }
  }
  XPC_LOG_OUTDENT();
#endif
}

void XPCWrappedNativeScope::DebugDump(int16_t depth) {
#ifdef DEBUG
  depth--;
  XPC_LOG_ALWAYS(("XPCWrappedNativeScope @ %p", this));
  XPC_LOG_INDENT();
  XPC_LOG_ALWAYS(("next @ %p", getNext()));
  XPC_LOG_ALWAYS(("mComponents @ %p", mComponents.get()));
  XPC_LOG_ALWAYS(("mCompartment @ %p", mCompartment));

  XPC_LOG_ALWAYS(("mWrappedNativeMap @ %p with %d wrappers(s)",
                  mWrappedNativeMap.get(), mWrappedNativeMap->Count()));
  // iterate contexts...
  if (depth && mWrappedNativeMap->Count()) {
    XPC_LOG_INDENT();
    for (auto i = mWrappedNativeMap->Iter(); !i.done(); i.next()) {
      i.get().value()->DebugDump(depth);
    }
    XPC_LOG_OUTDENT();
  }

  XPC_LOG_ALWAYS(("mWrappedNativeProtoMap @ %p with %d protos(s)",
                  mWrappedNativeProtoMap.get(),
                  mWrappedNativeProtoMap->Count()));
  // iterate contexts...
  if (depth && mWrappedNativeProtoMap->Count()) {
    XPC_LOG_INDENT();
    for (auto i = mWrappedNativeProtoMap->Iter(); !i.done(); i.next()) {
      i.get().value()->DebugDump(depth);
    }
    XPC_LOG_OUTDENT();
  }
  XPC_LOG_OUTDENT();
#endif
}

void XPCWrappedNativeScope::AddSizeOfAllScopesIncludingThis(
    JSContext* cx, ScopeSizeInfo* scopeSizeInfo) {
  for (XPCWrappedNativeScope* cur : AllScopes()) {
    cur->AddSizeOfIncludingThis(cx, scopeSizeInfo);
  }
}

void XPCWrappedNativeScope::AddSizeOfIncludingThis(
    JSContext* cx, ScopeSizeInfo* scopeSizeInfo) {
  scopeSizeInfo->mScopeAndMapSize += scopeSizeInfo->mMallocSizeOf(this);
  scopeSizeInfo->mScopeAndMapSize +=
      mWrappedNativeMap->SizeOfIncludingThis(scopeSizeInfo->mMallocSizeOf);
  scopeSizeInfo->mScopeAndMapSize +=
      mWrappedNativeProtoMap->SizeOfIncludingThis(scopeSizeInfo->mMallocSizeOf);

  auto realmCb = [](JSContext*, void* aData, JS::Realm* aRealm,
                    const JS::AutoRequireNoGC& nogc) {
    auto* scopeSizeInfo = static_cast<ScopeSizeInfo*>(aData);
    JSObject* global = GetRealmGlobalOrNull(aRealm);
    if (global && dom::HasProtoAndIfaceCache(global)) {
      dom::ProtoAndIfaceCache* cache = dom::GetProtoAndIfaceCache(global);
      scopeSizeInfo->mProtoAndIfaceCacheSize +=
          cache->SizeOfIncludingThis(scopeSizeInfo->mMallocSizeOf);
    }
  };
  IterateRealmsInCompartment(cx, Compartment(), scopeSizeInfo, realmCb);

  // There are other XPCWrappedNativeScope members that could be measured;
  // the above ones have been seen by DMD to be worth measuring.  More stuff
  // may be added later.
}
