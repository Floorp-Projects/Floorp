/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class used to manage the wrapped native objects within a JS scope. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "ExpandedPrincipal.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "nsIXULRuntime.h"
#include "mozJSComponentLoader.h"

#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace xpc;
using namespace JS;

/***************************************************************************/

XPCWrappedNativeScope* XPCWrappedNativeScope::gScopes = nullptr;
XPCWrappedNativeScope* XPCWrappedNativeScope::gDyingScopes = nullptr;

static bool RemoteXULForbidsXBLScope(HandleObject aFirstGlobal) {
  MOZ_ASSERT(aFirstGlobal);

  // Certain singleton sandoxes are created very early in startup - too early
  // to call into AllowXULXBLForPrincipal. We never create XBL scopes for
  // sandboxes anway, and certainly not for these singleton scopes. So we just
  // short-circuit here.
  if (IsSandbox(aFirstGlobal)) {
    return false;
  }

  // AllowXULXBLForPrincipal will return true for system principal, but we
  // don't want that here.
  nsIPrincipal* principal = xpc::GetObjectPrincipal(aFirstGlobal);
  MOZ_ASSERT(nsContentUtils::IsInitialized());
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return false;
  }

  // If this domain isn't whitelisted, we're done.
  if (!nsContentUtils::AllowXULXBLForPrincipal(principal)) {
    return false;
  }

  // Check the pref to determine how we should behave.
  return !Preferences::GetBool("dom.use_xbl_scopes_for_remote_xul", false);
}

XPCWrappedNativeScope::XPCWrappedNativeScope(JS::Compartment* aCompartment,
                                             JS::HandleObject aFirstGlobal)
    : mWrappedNativeMap(Native2WrappedNativeMap::newMap(XPC_NATIVE_MAP_LENGTH)),
      mWrappedNativeProtoMap(
          ClassInfo2WrappedNativeProtoMap::newMap(XPC_NATIVE_PROTO_MAP_LENGTH)),
      mComponents(nullptr),
      mNext(nullptr),
      mCompartment(aCompartment) {
#ifdef DEBUG
  for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
    MOZ_ASSERT(aCompartment != cur->Compartment(), "dup object");
  }
#endif

  // add ourselves to the scopes list
  mNext = gScopes;
  gScopes = this;

  MOZ_COUNT_CTOR(XPCWrappedNativeScope);

  // Determine whether we would allow an XBL scope in this situation.
  // In addition to being pref-controlled, we also disable XBL scopes for
  // remote XUL domains, _except_ if we have an additional pref override set.
  //
  // Note that we can't quite remove this yet, even though we never actually
  // use XBL scopes, because some code (including the security manager) uses
  // this boolean to make decisions that we rely on in our test infrastructure.
  mAllowContentXBLScope = !RemoteXULForbidsXBLScope(aFirstGlobal);
}

// static
bool XPCWrappedNativeScope::IsDyingScope(XPCWrappedNativeScope* scope) {
  for (XPCWrappedNativeScope* cur = gDyingScopes; cur; cur = cur->mNext) {
    if (scope == cur) {
      return true;
    }
  }
  return false;
}

bool XPCWrappedNativeScope::GetComponentsJSObject(JS::MutableHandleObject obj) {
  AutoJSContext cx;
  if (!mComponents) {
    bool system = AccessCheck::isChrome(mCompartment);
    mComponents =
        system ? new nsXPCComponents(this) : new nsXPCComponentsBase(this);
  }

  RootedValue val(cx);
  xpcObjectHelper helper(mComponents);
  bool ok = XPCConvert::NativeInterface2JSObject(&val, helper, nullptr, false,
                                                 nullptr);
  if (NS_WARN_IF(!ok)) {
    return false;
  }

  if (NS_WARN_IF(!val.isObject())) {
    return false;
  }

  // The call to wrap() here is necessary even though the object is same-
  // compartment, because it applies our security wrapper.
  obj.set(&val.toObject());
  if (NS_WARN_IF(!JS_WrapObject(cx, obj))) {
    return false;
  }
  return true;
}

void XPCWrappedNativeScope::ForcePrivilegedComponents() {
  nsCOMPtr<nsIXPCComponents> c = do_QueryInterface(mComponents);
  if (!c) {
    mComponents = new nsXPCComponents(this);
  }
}

static bool DefineSubcomponentProperty(JSContext* aCx, HandleObject aGlobal,
                                       nsISupports* aSubcomponent,
                                       const nsID* aIID,
                                       unsigned int aStringIndex) {
  RootedValue subcompVal(aCx);
  xpcObjectHelper helper(aSubcomponent);
  if (!XPCConvert::NativeInterface2JSObject(&subcompVal, helper, aIID, false,
                                            nullptr))
    return false;
  if (NS_WARN_IF(!subcompVal.isObject())) {
    return false;
  }
  RootedId id(aCx, XPCJSContext::Get()->GetStringID(aStringIndex));
  return JS_DefinePropertyById(aCx, aGlobal, id, subcompVal, 0);
}

bool XPCWrappedNativeScope::AttachComponentsObject(JSContext* aCx) {
  RootedObject components(aCx);
  if (!GetComponentsJSObject(&components)) {
    return false;
  }

  RootedObject global(aCx, CurrentGlobalOrNull(aCx));

  // The global Components property is non-configurable if it's a full
  // nsXPCComponents object. That way, if it's an nsXPCComponentsBase,
  // enableUniversalXPConnect can upgrade it later.
  unsigned attrs = JSPROP_READONLY | JSPROP_RESOLVING;
  nsCOMPtr<nsIXPCComponents> c = do_QueryInterface(mComponents);
  if (c) {
    attrs |= JSPROP_PERMANENT;
  }

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

  if (!c) {
    return true;
  }

  DEFINE_SUBCOMPONENT_PROPERTY(c, Classes, nullptr, CC)
  DEFINE_SUBCOMPONENT_PROPERTY(c, Utils, &NS_GET_IID(nsIXPCComponents_Utils),
                               CU)

#undef DEFINE_SUBCOMPONENT_PROPERTY

  return true;
}

JSObject* XPCWrappedNativeScope::EnsureContentXBLScope(JSContext* cx) {
  JS::RootedObject global(cx, CurrentGlobalOrNull(cx));
  MOZ_ASSERT(js::IsObjectInContextCompartment(global, cx));
  MOZ_ASSERT(!IsContentXBLScope());
  MOZ_ASSERT(strcmp(js::GetObjectClass(global)->name,
                    "nsXBLPrototypeScript compilation scope"));

  // We can probably remove EnsureContentXBLScope and clean up all its callers,
  // but a bunch (all?) of those callers will just go away when we remove XBL
  // support, so it's simpler to just leave it here as a no-op.
  return global;
}

bool XPCWrappedNativeScope::AllowContentXBLScope(Realm* aRealm) {
  // We only disallow XBL scopes in remote XUL situations.
  MOZ_ASSERT_IF(!mAllowContentXBLScope, nsContentUtils::AllowXULXBLForPrincipal(
                                            xpc::GetRealmPrincipal(aRealm)));
  return mAllowContentXBLScope;
}

namespace xpc {
JSObject* GetXBLScope(JSContext* cx, JSObject* contentScopeArg) {
  JS::RootedObject contentScope(cx, contentScopeArg);
  JSAutoRealm ar(cx, contentScope);
  XPCWrappedNativeScope* nativeScope =
      CompartmentPrivate::Get(contentScope)->scope;

  RootedObject scope(cx, nativeScope->EnsureContentXBLScope(cx));
  NS_ENSURE_TRUE(scope, nullptr);  // See bug 858642.

  scope = js::UncheckedUnwrap(scope);
  JS::ExposeObjectToActiveJS(scope);
  return scope;
}

JSObject* GetUAWidgetScope(JSContext* cx, JSObject* contentScopeArg) {
  JS::RootedObject contentScope(cx, contentScopeArg);
  JSAutoRealm ar(cx, contentScope);
  nsIPrincipal* principal = GetObjectPrincipal(contentScope);

  if (nsContentUtils::IsSystemPrincipal(principal)) {
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
  XPCWrappedNativeScope* scope = CompartmentPrivate::Get(comp)->scope;
  return scope && scope->AllowContentXBLScope(realm);
}

} /* namespace xpc */

XPCWrappedNativeScope::~XPCWrappedNativeScope() {
  MOZ_COUNT_DTOR(XPCWrappedNativeScope);

  // We can do additional cleanup assertions here...

  MOZ_ASSERT(0 == mWrappedNativeMap->Count(), "scope has non-empty map");
  delete mWrappedNativeMap;

  MOZ_ASSERT(0 == mWrappedNativeProtoMap->Count(), "scope has non-empty map");
  delete mWrappedNativeProtoMap;

  // This should not be necessary, since the Components object should die
  // with the scope but just in case.
  if (mComponents) {
    mComponents->mScope = nullptr;
  }

  // XXX we should assert that we are dead or that xpconnect has shutdown
  // XXX might not want to do this at xpconnect shutdown time???
  mComponents = nullptr;

  if (mXrayExpandos.initialized()) {
    mXrayExpandos.destroy();
  }

  JSContext* cx = dom::danger::GetJSContext();
  mIDProto.finalize(cx);
  mIIDProto.finalize(cx);
  mCIDProto.finalize(cx);
  mCompartment = nullptr;
}

// static
void XPCWrappedNativeScope::TraceWrappedNativesInAllScopes(JSTracer* trc) {
  // Do JS::TraceEdge for all wrapped natives with external references, as
  // well as any DOM expando objects.
  for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
      auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
      XPCWrappedNative* wrapper = entry->value;
      if (wrapper->HasExternalReference() && !wrapper->IsWrapperExpired()) {
        wrapper->TraceSelf(trc);
      }
    }
  }
}

// static
void XPCWrappedNativeScope::SuspectAllWrappers(
    nsCycleCollectionNoteRootCallback& cb) {
  for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
      static_cast<Native2WrappedNativeMap::Entry*>(i.Get())->value->Suspect(cb);
    }
  }
}

// static
void XPCWrappedNativeScope::UpdateWeakPointersInAllScopesAfterGC() {
  // If this is called from the finalization callback in JSGC_MARK_END then
  // JSGC_FINALIZE_END must always follow it calling
  // FinishedFinalizationPhaseOfGC and clearing gDyingScopes in
  // KillDyingScopes.
  MOZ_ASSERT(!gDyingScopes, "JSGC_MARK_END without JSGC_FINALIZE_END");

  XPCWrappedNativeScope** scopep = &gScopes;
  while (*scopep) {
    XPCWrappedNativeScope* cur = *scopep;
    cur->UpdateWeakPointersAfterGC();
    if (cur->Compartment()) {
      scopep = &cur->mNext;
    } else {
      // The scope's global is dead so move it to the dying scopes list.
      *scopep = cur->mNext;
      cur->mNext = gDyingScopes;
      gDyingScopes = cur;
    }
  }
}

void XPCWrappedNativeScope::UpdateWeakPointersAfterGC() {
  // Sweep waivers.
  if (mWaiverWrapperMap) {
    mWaiverWrapperMap->Sweep();
  }

  if (!js::IsCompartmentZoneSweepingOrCompacting(mCompartment)) {
    return;
  }

  // Update our pointer to the compartment in case we finalized all globals.
  if (js::gc::AllRealmsNeedSweep(mCompartment)) {
    mCompartment = nullptr;
    GetWrappedNativeMap()->Clear();
    mWrappedNativeProtoMap->Clear();
    return;
  }

  // Sweep mWrappedNativeMap for dying flat JS objects. Moving has already
  // been handled by XPCWrappedNative::FlatJSObjectMoved.
  for (auto iter = GetWrappedNativeMap()->Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<Native2WrappedNativeMap::Entry*>(iter.Get());
    XPCWrappedNative* wrapper = entry->value;
    JSObject* obj = wrapper->GetFlatJSObjectPreserveColor();
    JS_UpdateWeakPointerAfterGCUnbarriered(&obj);
    MOZ_ASSERT(!obj || obj == wrapper->GetFlatJSObjectPreserveColor());
    MOZ_ASSERT_IF(obj, js::GetObjectCompartment(obj) == mCompartment);
    if (!obj) {
      iter.Remove();
    }
  }

  // Sweep mWrappedNativeProtoMap for dying prototype JSObjects. Moving has
  // already been handled by XPCWrappedNativeProto::JSProtoObjectMoved.
  for (auto i = mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
    auto entry = static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
    JSObject* obj = entry->value->GetJSProtoObjectPreserveColor();
    JS_UpdateWeakPointerAfterGCUnbarriered(&obj);
    MOZ_ASSERT_IF(obj, js::GetObjectCompartment(obj) == mCompartment);
    MOZ_ASSERT(!obj || obj == entry->value->GetJSProtoObjectPreserveColor());
    if (!obj) {
      i.Remove();
    }
  }
}

// static
void XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs() {
  for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
      auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
      entry->value->SweepTearOffs();
    }
  }
}

// static
void XPCWrappedNativeScope::KillDyingScopes() {
  XPCWrappedNativeScope* cur = gDyingScopes;
  while (cur) {
    XPCWrappedNativeScope* next = cur->mNext;
    if (cur->Compartment()) {
      CompartmentPrivate::Get(cur->Compartment())->scope = nullptr;
    }
    delete cur;
    cur = next;
  }
  gDyingScopes = nullptr;
}

// static
void XPCWrappedNativeScope::SystemIsBeingShutDown() {
  int liveScopeCount = 0;

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
  gScopes = nullptr;

  // We're forcibly killing scopes, rather than allowing them to go away
  // when they're ready. As such, we need to do some cleanup before they
  // can safely be destroyed.

  for (cur = gDyingScopes; cur; cur = cur->mNext) {
    // Give the Components object a chance to try to clean up.
    if (cur->mComponents) {
      cur->mComponents->SystemIsBeingShutDown();
    }

    // Walk the protos first. Wrapper shutdown can leave dangling
    // proto pointers in the proto map.
    for (auto i = cur->mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
      auto entry =
          static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
      entry->value->SystemIsBeingShutDown();
      i.Remove();
    }
    for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
      auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
      XPCWrappedNative* wrapper = entry->value;
      if (wrapper->IsValid()) {
        wrapper->SystemIsBeingShutDown();
      }
      i.Remove();
    }

    CompartmentPrivate* priv = CompartmentPrivate::Get(cur->Compartment());
    priv->SystemIsBeingShutDown();
  }

  // Now it is safe to kill all the scopes.
  KillDyingScopes();
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
  XPCWrappedNativeScope* cur;
  for (cur = gScopes; cur; cur = cur->mNext) {
    count++;
  }

  XPC_LOG_ALWAYS(("chain of %d XPCWrappedNativeScope(s)", count));
  XPC_LOG_INDENT();
  XPC_LOG_ALWAYS(("gDyingScopes @ %p", gDyingScopes));
  if (depth) {
    for (cur = gScopes; cur; cur = cur->mNext) {
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
  XPC_LOG_ALWAYS(("mNext @ %p", mNext));
  XPC_LOG_ALWAYS(("mComponents @ %p", mComponents.get()));
  XPC_LOG_ALWAYS(("mCompartment @ %p", mCompartment));

  XPC_LOG_ALWAYS(("mWrappedNativeMap @ %p with %d wrappers(s)",
                  mWrappedNativeMap, mWrappedNativeMap->Count()));
  // iterate contexts...
  if (depth && mWrappedNativeMap->Count()) {
    XPC_LOG_INDENT();
    for (auto i = mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
      auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
      entry->value->DebugDump(depth);
    }
    XPC_LOG_OUTDENT();
  }

  XPC_LOG_ALWAYS(("mWrappedNativeProtoMap @ %p with %d protos(s)",
                  mWrappedNativeProtoMap, mWrappedNativeProtoMap->Count()));
  // iterate contexts...
  if (depth && mWrappedNativeProtoMap->Count()) {
    XPC_LOG_INDENT();
    for (auto i = mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
      auto entry =
          static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
      entry->value->DebugDump(depth);
    }
    XPC_LOG_OUTDENT();
  }
  XPC_LOG_OUTDENT();
#endif
}

void XPCWrappedNativeScope::AddSizeOfAllScopesIncludingThis(
    JSContext* cx, ScopeSizeInfo* scopeSizeInfo) {
  for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
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

  auto realmCb = [](JSContext*, void* aData, JS::Handle<JS::Realm*> aRealm) {
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
