/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class used to manage the wrapped native objects within a JS scope. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsPrincipal.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "nsIAddonInterposition.h"
#include "nsIXULRuntime.h"

#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace xpc;
using namespace JS;

/***************************************************************************/

XPCWrappedNativeScope* XPCWrappedNativeScope::gScopes = nullptr;
XPCWrappedNativeScope* XPCWrappedNativeScope::gDyingScopes = nullptr;
bool XPCWrappedNativeScope::gShutdownObserverInitialized = false;
XPCWrappedNativeScope::InterpositionMap* XPCWrappedNativeScope::gInterpositionMap = nullptr;
InterpositionWhitelistArray* XPCWrappedNativeScope::gInterpositionWhitelists = nullptr;
XPCWrappedNativeScope::AddonSet* XPCWrappedNativeScope::gAllowCPOWAddonSet = nullptr;

NS_IMPL_ISUPPORTS(XPCWrappedNativeScope::ClearInterpositionsObserver, nsIObserver)

NS_IMETHODIMP
XPCWrappedNativeScope::ClearInterpositionsObserver::Observe(nsISupports* subject,
                                                            const char* topic,
                                                            const char16_t* data)
{
    MOZ_ASSERT(strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    // The interposition map holds strong references to interpositions, which
    // may themselves be involved in cycles. We need to drop these strong
    // references before the cycle collector shuts down. Otherwise we'll
    // leak. This observer always runs before CC shutdown.
    if (gInterpositionMap) {
        delete gInterpositionMap;
        gInterpositionMap = nullptr;
    }

    if (gInterpositionWhitelists) {
        delete gInterpositionWhitelists;
        gInterpositionWhitelists = nullptr;
    }

    if (gAllowCPOWAddonSet) {
        delete gAllowCPOWAddonSet;
        gAllowCPOWAddonSet = nullptr;
    }

    nsContentUtils::UnregisterShutdownObserver(this);
    return NS_OK;
}

static bool
RemoteXULForbidsXBLScope(nsIPrincipal* aPrincipal, HandleObject aGlobal)
{
  MOZ_ASSERT(aPrincipal);

  // Certain singleton sandoxes are created very early in startup - too early
  // to call into AllowXULXBLForPrincipal. We never create XBL scopes for
  // sandboxes anway, and certainly not for these singleton scopes. So we just
  // short-circuit here.
  if (IsSandbox(aGlobal))
      return false;

  // AllowXULXBLForPrincipal will return true for system principal, but we
  // don't want that here.
  MOZ_ASSERT(nsContentUtils::IsInitialized());
  if (nsContentUtils::IsSystemPrincipal(aPrincipal))
      return false;

  // If this domain isn't whitelisted, we're done.
  if (!nsContentUtils::AllowXULXBLForPrincipal(aPrincipal))
      return false;

  // Check the pref to determine how we should behave.
  return !Preferences::GetBool("dom.use_xbl_scopes_for_remote_xul", false);
}

XPCWrappedNativeScope::XPCWrappedNativeScope(JSContext* cx,
                                             JS::HandleObject aGlobal)
      : mWrappedNativeMap(Native2WrappedNativeMap::newMap(XPC_NATIVE_MAP_LENGTH)),
        mWrappedNativeProtoMap(ClassInfo2WrappedNativeProtoMap::newMap(XPC_NATIVE_PROTO_MAP_LENGTH)),
        mComponents(nullptr),
        mNext(nullptr),
        mGlobalJSObject(aGlobal),
        mHasCallInterpositions(false),
        mIsContentXBLScope(false),
        mIsAddonScope(false)
{
    // add ourselves to the scopes list
    {
        MOZ_ASSERT(aGlobal);
        DebugOnly<const js::Class*> clasp = js::GetObjectClass(aGlobal);
        MOZ_ASSERT(clasp->flags & (JSCLASS_PRIVATE_IS_NSISUPPORTS |
                                   JSCLASS_HAS_PRIVATE) ||
                   mozilla::dom::IsDOMClass(clasp));
#ifdef DEBUG
        for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext)
            MOZ_ASSERT(aGlobal != cur->GetGlobalJSObjectPreserveColor(), "dup object");
#endif

        mNext = gScopes;
        gScopes = this;
    }

    MOZ_COUNT_CTOR(XPCWrappedNativeScope);

    // Create the compartment private.
    JSCompartment* c = js::GetObjectCompartment(aGlobal);
    MOZ_ASSERT(!JS_GetCompartmentPrivate(c));
    CompartmentPrivate* priv = new CompartmentPrivate(c);
    JS_SetCompartmentPrivate(c, priv);

    // Attach ourselves to the compartment private.
    priv->scope = this;

    // Determine whether we would allow an XBL scope in this situation.
    // In addition to being pref-controlled, we also disable XBL scopes for
    // remote XUL domains, _except_ if we have an additional pref override set.
    nsIPrincipal* principal = GetPrincipal();
    mAllowContentXBLScope = !RemoteXULForbidsXBLScope(principal, aGlobal);

    // Determine whether to use an XBL scope.
    mUseContentXBLScope = mAllowContentXBLScope;
    if (mUseContentXBLScope) {
      const js::Class* clasp = js::GetObjectClass(mGlobalJSObject);
      mUseContentXBLScope = !strcmp(clasp->name, "Window");
    }
    if (mUseContentXBLScope) {
      mUseContentXBLScope = principal && !nsContentUtils::IsSystemPrincipal(principal);
    }

    JSAddonId* addonId = JS::AddonIdOfObject(aGlobal);
    if (gInterpositionMap) {
        bool isSystem = nsContentUtils::IsSystemPrincipal(principal);
        bool waiveInterposition = priv->waiveInterposition;
        InterpositionMap::Ptr interposition = gInterpositionMap->lookup(addonId);
        if (!waiveInterposition && interposition) {
            MOZ_RELEASE_ASSERT(isSystem);
            mInterposition = interposition->value();
        }
        // We also want multiprocessCompatible add-ons to have a default interposition.
        if (!mInterposition && addonId && isSystem) {
          bool interpositionEnabled = mozilla::Preferences::GetBool(
            "extensions.interposition.enabled", false);
          if (interpositionEnabled) {
            mInterposition = do_GetService("@mozilla.org/addons/default-addon-shims;1");
            MOZ_ASSERT(mInterposition);
            UpdateInterpositionWhitelist(cx, mInterposition);
          }
        }
    }

    if (addonId) {
        // We forbid CPOWs unless they're specifically allowed.
        priv->allowCPOWs = gAllowCPOWAddonSet ? gAllowCPOWAddonSet->has(addonId) : false;
    }
}

// static
bool
XPCWrappedNativeScope::IsDyingScope(XPCWrappedNativeScope* scope)
{
    for (XPCWrappedNativeScope* cur = gDyingScopes; cur; cur = cur->mNext) {
        if (scope == cur)
            return true;
    }
    return false;
}

bool
XPCWrappedNativeScope::GetComponentsJSObject(JS::MutableHandleObject obj)
{
    AutoJSContext cx;
    if (!mComponents) {
        nsIPrincipal* p = GetPrincipal();
        bool system = nsXPConnect::SecurityManager()->IsSystemPrincipal(p);
        mComponents = system ? new nsXPCComponents(this)
                             : new nsXPCComponentsBase(this);
    }

    RootedValue val(cx);
    xpcObjectHelper helper(mComponents);
    bool ok = XPCConvert::NativeInterface2JSObject(&val, nullptr, helper,
                                                   nullptr, false,
                                                   nullptr);
    if (NS_WARN_IF(!ok))
        return false;

    if (NS_WARN_IF(!val.isObject()))
        return false;

    // The call to wrap() here is necessary even though the object is same-
    // compartment, because it applies our security wrapper.
    obj.set(&val.toObject());
    if (NS_WARN_IF(!JS_WrapObject(cx, obj)))
        return false;
    return true;
}

void
XPCWrappedNativeScope::ForcePrivilegedComponents()
{
    nsCOMPtr<nsIXPCComponents> c = do_QueryInterface(mComponents);
    if (!c)
        mComponents = new nsXPCComponents(this);
}

bool
XPCWrappedNativeScope::AttachComponentsObject(JSContext* aCx)
{
    RootedObject components(aCx);
    if (!GetComponentsJSObject(&components))
        return false;

    RootedObject global(aCx, GetGlobalJSObject());
    MOZ_ASSERT(js::IsObjectInContextCompartment(global, aCx));

    // The global Components property is non-configurable if it's a full
    // nsXPCComponents object. That way, if it's an nsXPCComponentsBase,
    // enableUniversalXPConnect can upgrade it later.
    unsigned attrs = JSPROP_READONLY | JSPROP_RESOLVING;
    nsCOMPtr<nsIXPCComponents> c = do_QueryInterface(mComponents);
    if (c)
        attrs |= JSPROP_PERMANENT;

    RootedId id(aCx, XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_COMPONENTS));
    return JS_DefinePropertyById(aCx, global, id, components, attrs);
}

static bool
CompartmentPerAddon()
{
    static bool initialized = false;
    static bool pref = false;

    if (!initialized) {
        pref = Preferences::GetBool("dom.compartment_per_addon", false) ||
               BrowserTabsRemoteAutostart();
        initialized = true;
    }

    return pref;
}

JSObject*
XPCWrappedNativeScope::EnsureContentXBLScope(JSContext* cx)
{
    JS::RootedObject global(cx, GetGlobalJSObject());
    MOZ_ASSERT(js::IsObjectInContextCompartment(global, cx));
    MOZ_ASSERT(!mIsContentXBLScope);
    MOZ_ASSERT(strcmp(js::GetObjectClass(global)->name,
                      "nsXBLPrototypeScript compilation scope"));

    // If we already have a special XBL scope object, we know what to use.
    if (mContentXBLScope)
        return mContentXBLScope;

    // If this scope doesn't need an XBL scope, just return the global.
    if (!mUseContentXBLScope)
        return global;

    // Set up the sandbox options. Note that we use the DOM global as the
    // sandboxPrototype so that the XBL scope can access all the DOM objects
    // it's accustomed to accessing.
    //
    // In general wantXrays shouldn't matter much here, but there are weird
    // cases when adopting bound content between same-origin globals where a
    // <destructor> in one content XBL scope sees anonymous content in another
    // content XBL scope. When that happens, we hit LookupBindingMember for an
    // anonymous element that lives in a content XBL scope, which isn't a tested
    // or audited codepath. So let's avoid hitting that case by opting out of
    // same-origin Xrays.
    SandboxOptions options;
    options.wantXrays = false;
    options.wantComponents = true;
    options.proto = global;
    options.sameZoneAs = global;

    // Use an nsExpandedPrincipal to create asymmetric security.
    nsIPrincipal* principal = GetPrincipal();
    MOZ_ASSERT(!nsContentUtils::IsExpandedPrincipal(principal));
    nsTArray< nsCOMPtr<nsIPrincipal> > principalAsArray(1);
    principalAsArray.AppendElement(principal);
    nsCOMPtr<nsIExpandedPrincipal> ep = new nsExpandedPrincipal(principalAsArray);

    // Create the sandbox.
    RootedValue v(cx);
    nsresult rv = CreateSandboxObject(cx, &v, ep, options);
    NS_ENSURE_SUCCESS(rv, nullptr);
    mContentXBLScope = &v.toObject();

    // Tag it.
    CompartmentPrivate::Get(js::UncheckedUnwrap(mContentXBLScope))->scope->mIsContentXBLScope = true;

    // Good to go!
    return mContentXBLScope;
}

bool
XPCWrappedNativeScope::AllowContentXBLScope()
{
    // We only disallow XBL scopes in remote XUL situations.
    MOZ_ASSERT_IF(!mAllowContentXBLScope,
                  nsContentUtils::AllowXULXBLForPrincipal(GetPrincipal()));
    return mAllowContentXBLScope;
}

namespace xpc {
JSObject*
GetXBLScope(JSContext* cx, JSObject* contentScopeArg)
{
    MOZ_ASSERT(!IsInAddonScope(contentScopeArg));

    JS::RootedObject contentScope(cx, contentScopeArg);
    JSAutoCompartment ac(cx, contentScope);
    JSObject* scope = CompartmentPrivate::Get(contentScope)->scope->EnsureContentXBLScope(cx);
    NS_ENSURE_TRUE(scope, nullptr); // See bug 858642.
    scope = js::UncheckedUnwrap(scope);
    JS::ExposeObjectToActiveJS(scope);
    return scope;
}

JSObject*
GetScopeForXBLExecution(JSContext* cx, HandleObject contentScope, JSAddonId* addonId)
{
    MOZ_RELEASE_ASSERT(!IsInAddonScope(contentScope));

    RootedObject global(cx, js::GetGlobalForObjectCrossCompartment(contentScope));
    if (IsInContentXBLScope(contentScope))
        return global;

    JSAutoCompartment ac(cx, contentScope);
    XPCWrappedNativeScope* nativeScope = CompartmentPrivate::Get(contentScope)->scope;
    bool isSystem = nsContentUtils::IsSystemPrincipal(nativeScope->GetPrincipal());

    RootedObject scope(cx);
    if (nativeScope->UseContentXBLScope())
        scope = nativeScope->EnsureContentXBLScope(cx);
    else if (addonId && CompartmentPerAddon() && isSystem)
        scope = nativeScope->EnsureAddonScope(cx, addonId);
    else
        scope = global;

    NS_ENSURE_TRUE(scope, nullptr); // See bug 858642.
    scope = js::UncheckedUnwrap(scope);
    JS::ExposeObjectToActiveJS(scope);
    return scope;
}

bool
AllowContentXBLScope(JSCompartment* c)
{
  XPCWrappedNativeScope* scope = CompartmentPrivate::Get(c)->scope;
  return scope && scope->AllowContentXBLScope();
}

bool
UseContentXBLScope(JSCompartment* c)
{
  XPCWrappedNativeScope* scope = CompartmentPrivate::Get(c)->scope;
  return scope && scope->UseContentXBLScope();
}

void
ClearContentXBLScope(JSObject* global)
{
    CompartmentPrivate::Get(global)->scope->ClearContentXBLScope();
}

} /* namespace xpc */

JSObject*
XPCWrappedNativeScope::EnsureAddonScope(JSContext* cx, JSAddonId* addonId)
{
    JS::RootedObject global(cx, GetGlobalJSObject());
    MOZ_ASSERT(js::IsObjectInContextCompartment(global, cx));
    MOZ_ASSERT(!mIsContentXBLScope);
    MOZ_ASSERT(!mIsAddonScope);
    MOZ_ASSERT(addonId);
    MOZ_ASSERT(nsContentUtils::IsSystemPrincipal(GetPrincipal()));

    // In bug 1092156, we found that add-on scopes don't work correctly when the
    // window navigates. The add-on global's prototype is an outer window, so,
    // after the navigation, looking up window properties in the add-on scope
    // will fail. However, in most cases where the window can be navigated, the
    // entire window is part of the add-on. To solve the problem, we avoid
    // returning an add-on scope for a window that is already tagged with the
    // add-on ID.
    if (AddonIdOfObject(global) == addonId)
        return global;

    // If we already have an addon scope object, we know what to use.
    for (size_t i = 0; i < mAddonScopes.Length(); i++) {
        if (JS::AddonIdOfObject(js::UncheckedUnwrap(mAddonScopes[i])) == addonId)
            return mAddonScopes[i];
    }

    SandboxOptions options;
    options.wantComponents = true;
    options.proto = global;
    options.sameZoneAs = global;
    options.addonId = JS::StringOfAddonId(addonId);
    options.writeToGlobalPrototype = true;

    RootedValue v(cx);
    nsresult rv = CreateSandboxObject(cx, &v, GetPrincipal(), options);
    NS_ENSURE_SUCCESS(rv, nullptr);
    mAddonScopes.AppendElement(&v.toObject());

    CompartmentPrivate::Get(js::UncheckedUnwrap(&v.toObject()))->scope->mIsAddonScope = true;
    return &v.toObject();
}

JSObject*
xpc::GetAddonScope(JSContext* cx, JS::HandleObject contentScope, JSAddonId* addonId)
{
    MOZ_RELEASE_ASSERT(!IsInAddonScope(contentScope));

    if (!addonId || !CompartmentPerAddon()) {
        return js::GetGlobalForObjectCrossCompartment(contentScope);
    }

    JSAutoCompartment ac(cx, contentScope);
    XPCWrappedNativeScope* nativeScope = CompartmentPrivate::Get(contentScope)->scope;
    if (nativeScope->GetPrincipal() != nsXPConnect::SystemPrincipal()) {
        // This can happen if, for example, Jetpack loads an unprivileged HTML
        // page from the add-on. It's not clear what to do there, so we just use
        // the normal global.
        return js::GetGlobalForObjectCrossCompartment(contentScope);
    }
    JSObject* scope = nativeScope->EnsureAddonScope(cx, addonId);
    NS_ENSURE_TRUE(scope, nullptr);

    scope = js::UncheckedUnwrap(scope);
    JS::ExposeObjectToActiveJS(scope);
    return scope;
}

XPCWrappedNativeScope::~XPCWrappedNativeScope()
{
    MOZ_COUNT_DTOR(XPCWrappedNativeScope);

    // We can do additional cleanup assertions here...

    MOZ_ASSERT(0 == mWrappedNativeMap->Count(), "scope has non-empty map");
    delete mWrappedNativeMap;

    MOZ_ASSERT(0 == mWrappedNativeProtoMap->Count(), "scope has non-empty map");
    delete mWrappedNativeProtoMap;

    // This should not be necessary, since the Components object should die
    // with the scope but just in case.
    if (mComponents)
        mComponents->mScope = nullptr;

    // XXX we should assert that we are dead or that xpconnect has shutdown
    // XXX might not want to do this at xpconnect shutdown time???
    mComponents = nullptr;

    if (mXrayExpandos.initialized())
        mXrayExpandos.destroy();

    JSContext* cx = dom::danger::GetJSContext();
    mContentXBLScope.finalize(cx);
    for (size_t i = 0; i < mAddonScopes.Length(); i++)
        mAddonScopes[i].finalize(cx);
    mGlobalJSObject.finalize(cx);
}

// static
void
XPCWrappedNativeScope::TraceWrappedNativesInAllScopes(JSTracer* trc, XPCJSRuntime* rt)
{
    // Do JS::TraceEdge for all wrapped natives with external references, as
    // well as any DOM expando objects.
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
            XPCWrappedNative* wrapper = entry->value;
            if (wrapper->HasExternalReference() && !wrapper->IsWrapperExpired())
                wrapper->TraceSelf(trc);
        }

        if (cur->mDOMExpandoSet) {
            for (DOMExpandoSet::Enum e(*cur->mDOMExpandoSet); !e.empty(); e.popFront())
                JS::TraceEdge(trc, &e.mutableFront(), "DOM expando object");
        }
    }
}

static void
SuspectDOMExpandos(JSObject* obj, nsCycleCollectionNoteRootCallback& cb)
{
    MOZ_ASSERT(dom::GetDOMClass(obj) && dom::GetDOMClass(obj)->mDOMObjectIsISupports);
    nsISupports* native = dom::UnwrapDOMObject<nsISupports>(obj);
    cb.NoteXPCOMRoot(native);
}

// static
void
XPCWrappedNativeScope::SuspectAllWrappers(XPCJSRuntime* rt,
                                          nsCycleCollectionNoteRootCallback& cb)
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
            static_cast<Native2WrappedNativeMap::Entry*>(i.Get())->value->Suspect(cb);
        }

        if (cur->mDOMExpandoSet) {
            for (DOMExpandoSet::Range r = cur->mDOMExpandoSet->all(); !r.empty(); r.popFront())
                SuspectDOMExpandos(r.front(), cb);
        }
    }
}

// static
void
XPCWrappedNativeScope::UpdateWeakPointersAfterGC(XPCJSRuntime* rt)
{
    // If this is called from the finalization callback in JSGC_MARK_END then
    // JSGC_FINALIZE_END must always follow it calling
    // FinishedFinalizationPhaseOfGC and clearing gDyingScopes in
    // KillDyingScopes.
    MOZ_ASSERT(!gDyingScopes, "JSGC_MARK_END without JSGC_FINALIZE_END");

    XPCWrappedNativeScope* prev = nullptr;
    XPCWrappedNativeScope* cur = gScopes;

    while (cur) {
        // Sweep waivers.
        if (cur->mWaiverWrapperMap)
            cur->mWaiverWrapperMap->Sweep();

        XPCWrappedNativeScope* next = cur->mNext;

        if (cur->mContentXBLScope)
            cur->mContentXBLScope.updateWeakPointerAfterGC();
        for (size_t i = 0; i < cur->mAddonScopes.Length(); i++)
            cur->mAddonScopes[i].updateWeakPointerAfterGC();

        // Check for finalization of the global object or update our pointer if
        // it was moved.
        if (cur->mGlobalJSObject) {
            cur->mGlobalJSObject.updateWeakPointerAfterGC();
            if (!cur->mGlobalJSObject) {
                // Move this scope from the live list to the dying list.
                if (prev)
                    prev->mNext = next;
                else
                    gScopes = next;
                cur->mNext = gDyingScopes;
                gDyingScopes = cur;
                cur = nullptr;
            }
        }

        if (cur)
            prev = cur;
        cur = next;
    }
}

// static
void
XPCWrappedNativeScope::MarkAllWrappedNativesAndProtos()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
            entry->value->Mark();
        }
        // We need to explicitly mark all the protos too because some protos may be
        // alive in the hashtable but not currently in use by any wrapper
        for (auto i = cur->mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
            entry->value->Mark();
        }
    }
}

#ifdef DEBUG
// static
void
XPCWrappedNativeScope::ASSERT_NoInterfaceSetsAreMarked()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
            entry->value->ASSERT_SetsNotMarked();
        }
        for (auto i = cur->mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
            entry->value->ASSERT_SetNotMarked();
        }
    }
}
#endif

// static
void
XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        for (auto i = cur->mWrappedNativeMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<Native2WrappedNativeMap::Entry*>(i.Get());
            entry->value->SweepTearOffs();
        }
    }
}

// static
void
XPCWrappedNativeScope::KillDyingScopes()
{
    XPCWrappedNativeScope* cur = gDyingScopes;
    while (cur) {
        XPCWrappedNativeScope* next = cur->mNext;
        if (cur->mGlobalJSObject)
            CompartmentPrivate::Get(cur->mGlobalJSObject)->scope = nullptr;
        delete cur;
        cur = next;
    }
    gDyingScopes = nullptr;
}

//static
void
XPCWrappedNativeScope::SystemIsBeingShutDown()
{
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
        if (cur->mComponents)
            cur->mComponents->SystemIsBeingShutDown();

        // Walk the protos first. Wrapper shutdown can leave dangling
        // proto pointers in the proto map.
        for (auto i = cur->mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
            auto entry = static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
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
    }

    // Now it is safe to kill all the scopes.
    KillDyingScopes();
}


/***************************************************************************/

JSObject*
XPCWrappedNativeScope::GetExpandoChain(HandleObject target)
{
    MOZ_ASSERT(ObjectScope(target) == this);
    if (!mXrayExpandos.initialized())
        return nullptr;
    return mXrayExpandos.lookup(target);
}

bool
XPCWrappedNativeScope::SetExpandoChain(JSContext* cx, HandleObject target,
                                       HandleObject chain)
{
    MOZ_ASSERT(ObjectScope(target) == this);
    MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
    MOZ_ASSERT_IF(chain, ObjectScope(chain) == this);
    if (!mXrayExpandos.initialized() && !mXrayExpandos.init(cx))
        return false;
    return mXrayExpandos.put(cx, target, chain);
}

/* static */ bool
XPCWrappedNativeScope::SetAddonInterposition(JSContext* cx,
                                             JSAddonId* addonId,
                                             nsIAddonInterposition* interp)
{
    if (!gInterpositionMap) {
        gInterpositionMap = new InterpositionMap();
        bool ok = gInterpositionMap->init();
        NS_ENSURE_TRUE(ok, false);

        if (!gShutdownObserverInitialized) {
            gShutdownObserverInitialized = true;
            nsContentUtils::RegisterShutdownObserver(new ClearInterpositionsObserver());
        }
    }
    if (interp) {
        bool ok = gInterpositionMap->put(addonId, interp);
        NS_ENSURE_TRUE(ok, false);
        UpdateInterpositionWhitelist(cx, interp);
    } else {
        gInterpositionMap->remove(addonId);
    }
    return true;
}

/* static */ bool
XPCWrappedNativeScope::AllowCPOWsInAddon(JSContext* cx,
                                         JSAddonId* addonId,
                                         bool allow)
{
    if (!gAllowCPOWAddonSet) {
        gAllowCPOWAddonSet = new AddonSet();
        bool ok = gAllowCPOWAddonSet->init();
        NS_ENSURE_TRUE(ok, false);

        if (!gShutdownObserverInitialized) {
            gShutdownObserverInitialized = true;
            nsContentUtils::RegisterShutdownObserver(new ClearInterpositionsObserver());
        }
    }
    if (allow) {
        bool ok = gAllowCPOWAddonSet->put(addonId);
        NS_ENSURE_TRUE(ok, false);
    } else {
        gAllowCPOWAddonSet->remove(addonId);
    }
    return true;
}

nsCOMPtr<nsIAddonInterposition>
XPCWrappedNativeScope::GetInterposition()
{
    return mInterposition;
}

/* static */ InterpositionWhitelist*
XPCWrappedNativeScope::GetInterpositionWhitelist(nsIAddonInterposition* interposition)
{
    if (!gInterpositionWhitelists)
        return nullptr;

    InterpositionWhitelistArray& wls = *gInterpositionWhitelists;
    for (size_t i = 0; i < wls.Length(); i++) {
        if (wls[i].interposition == interposition)
            return &wls[i].whitelist;
    }

    return nullptr;
}

/* static */ bool
XPCWrappedNativeScope::UpdateInterpositionWhitelist(JSContext* cx,
                                                    nsIAddonInterposition* interposition)
{
    // We want to set the interpostion whitelist only once.
    InterpositionWhitelist* whitelist = GetInterpositionWhitelist(interposition);
    if (whitelist)
        return true;

    // The hashsets in gInterpositionWhitelists do not have a copy constructor so
    // a reallocation for the array will lead to a memory corruption. If you
    // need more interpositions, change the capacity of the array please.
    static const size_t MAX_INTERPOSITION = 8;
    if (!gInterpositionWhitelists)
        gInterpositionWhitelists = new InterpositionWhitelistArray(MAX_INTERPOSITION);

    MOZ_RELEASE_ASSERT(MAX_INTERPOSITION > gInterpositionWhitelists->Length() + 1);
    InterpositionWhitelistPair* newPair = gInterpositionWhitelists->AppendElement();
    newPair->interposition = interposition;
    if (!newPair->whitelist.init()) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    whitelist = &newPair->whitelist;

    RootedValue whitelistVal(cx);
    nsresult rv = interposition->GetWhitelist(&whitelistVal);
    if (NS_FAILED(rv)) {
        JS_ReportError(cx, "Could not get the whitelist from the interposition.");
        return false;
    }

    if (!whitelistVal.isObject()) {
        JS_ReportError(cx, "Whitelist must be an array.");
        return false;
    }

    // We want to enter the whitelist's compartment to avoid any wrappers.
    // To be on the safe side let's make sure that it's a system compartment
    // and we don't accidentally trigger some content function here by parsing
    // the whitelist object.
    RootedObject whitelistObj(cx, &whitelistVal.toObject());
    whitelistObj = js::UncheckedUnwrap(whitelistObj);
    if (!AccessCheck::isChrome(whitelistObj)) {
        JS_ReportError(cx, "Whitelist must be from system scope.");
        return false;
    }

    {
        JSAutoCompartment ac(cx, whitelistObj);

        bool isArray;
        if (!JS_IsArrayObject(cx, whitelistObj, &isArray))
            return false;

        if (!isArray) {
            JS_ReportError(cx, "Whitelist must be an array.");
            return false;
        }

        uint32_t length;
        if (!JS_GetArrayLength(cx, whitelistObj, &length))
            return false;

        for (uint32_t i = 0; i < length; i++) {
            RootedValue idval(cx);
            if (!JS_GetElement(cx, whitelistObj, i, &idval))
                return false;

            if (!idval.isString()) {
                JS_ReportError(cx, "Whitelist must contain strings only.");
                return false;
            }

            RootedString str(cx, idval.toString());
            str = JS_AtomizeAndPinJSString(cx, str);
            if (!str) {
                JS_ReportError(cx, "String internization failed.");
                return false;
            }

            // By internizing the id's we ensure that they won't get
            // GCed so we can use them as hash keys.
            jsid id = INTERNED_STRING_TO_JSID(cx, str);
            if (!whitelist->put(JSID_BITS(id))) {
                JS_ReportOutOfMemory(cx);
                return false;
            }
        }
    }

    return true;
}

/***************************************************************************/

// static
void
XPCWrappedNativeScope::DebugDumpAllScopes(int16_t depth)
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

void
XPCWrappedNativeScope::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("XPCWrappedNativeScope @ %x", this));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("mNext @ %x", mNext));
        XPC_LOG_ALWAYS(("mComponents @ %x", mComponents.get()));
        XPC_LOG_ALWAYS(("mGlobalJSObject @ %x", mGlobalJSObject.get()));

        XPC_LOG_ALWAYS(("mWrappedNativeMap @ %x with %d wrappers(s)",
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

        XPC_LOG_ALWAYS(("mWrappedNativeProtoMap @ %x with %d protos(s)",
                        mWrappedNativeProtoMap,
                        mWrappedNativeProtoMap->Count()));
        // iterate contexts...
        if (depth && mWrappedNativeProtoMap->Count()) {
            XPC_LOG_INDENT();
            for (auto i = mWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
                auto entry = static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(i.Get());
                entry->value->DebugDump(depth);
            }
            XPC_LOG_OUTDENT();
        }
    XPC_LOG_OUTDENT();
#endif
}

void
XPCWrappedNativeScope::AddSizeOfAllScopesIncludingThis(ScopeSizeInfo* scopeSizeInfo)
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext)
        cur->AddSizeOfIncludingThis(scopeSizeInfo);
}

void
XPCWrappedNativeScope::AddSizeOfIncludingThis(ScopeSizeInfo* scopeSizeInfo)
{
    scopeSizeInfo->mScopeAndMapSize += scopeSizeInfo->mMallocSizeOf(this);
    scopeSizeInfo->mScopeAndMapSize +=
        mWrappedNativeMap->SizeOfIncludingThis(scopeSizeInfo->mMallocSizeOf);
    scopeSizeInfo->mScopeAndMapSize +=
        mWrappedNativeProtoMap->SizeOfIncludingThis(scopeSizeInfo->mMallocSizeOf);

    if (dom::HasProtoAndIfaceCache(mGlobalJSObject)) {
        dom::ProtoAndIfaceCache* cache = dom::GetProtoAndIfaceCache(mGlobalJSObject);
        scopeSizeInfo->mProtoAndIfaceCacheSize +=
            cache->SizeOfIncludingThis(scopeSizeInfo->mMallocSizeOf);
    }

    // There are other XPCWrappedNativeScope members that could be measured;
    // the above ones have been seen by DMD to be worth measuring.  More stuff
    // may be added later.
}
