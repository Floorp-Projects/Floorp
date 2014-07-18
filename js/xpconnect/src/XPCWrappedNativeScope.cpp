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

#include "mozilla/dom/BindingUtils.h"

using namespace mozilla;
using namespace xpc;
using namespace JS;

/***************************************************************************/

XPCWrappedNativeScope* XPCWrappedNativeScope::gScopes = nullptr;
XPCWrappedNativeScope* XPCWrappedNativeScope::gDyingScopes = nullptr;
XPCWrappedNativeScope::InterpositionMap* XPCWrappedNativeScope::gInterpositionMap = nullptr;

static bool
RemoteXULForbidsXBLScope(nsIPrincipal *aPrincipal, HandleObject aGlobal)
{
  MOZ_ASSERT(aPrincipal);

  // The SafeJSContext is lazily created, and tends to be created at really
  // weird times, at least for xpcshell (often very early in startup or late
  // in shutdown). Its scope isn't system principal, so if we proceeded we'd
  // end up calling into AllowXULXBLForPrincipal, which depends on all kinds
  // of persistent storage and permission machinery that may or not be running.
  // We know the answer to the question here, so just short-circuit.
  if (JS_GetClass(aGlobal) == &SafeJSContextGlobalClass)
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

XPCWrappedNativeScope::XPCWrappedNativeScope(JSContext *cx,
                                             JS::HandleObject aGlobal)
      : mWrappedNativeMap(Native2WrappedNativeMap::newMap(XPC_NATIVE_MAP_SIZE)),
        mWrappedNativeProtoMap(ClassInfo2WrappedNativeProtoMap::newMap(XPC_NATIVE_PROTO_MAP_SIZE)),
        mComponents(nullptr),
        mNext(nullptr),
        mGlobalJSObject(aGlobal),
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
    JSCompartment *c = js::GetObjectCompartment(aGlobal);
    MOZ_ASSERT(!JS_GetCompartmentPrivate(c));
    CompartmentPrivate *priv = new CompartmentPrivate(c);
    JS_SetCompartmentPrivate(c, priv);

    // Attach ourselves to the compartment private.
    priv->scope = this;

    // Determine whether we would allow an XBL scope in this situation.
    // In addition to being pref-controlled, we also disable XBL scopes for
    // remote XUL domains, _except_ if we have an additional pref override set.
    nsIPrincipal *principal = GetPrincipal();
    mAllowContentXBLScope = !RemoteXULForbidsXBLScope(principal, aGlobal);

    // Determine whether to use an XBL scope.
    mUseContentXBLScope = mAllowContentXBLScope;
    if (mUseContentXBLScope) {
      const js::Class *clasp = js::GetObjectClass(mGlobalJSObject);
      mUseContentXBLScope = !strcmp(clasp->name, "Window") ||
                            !strcmp(clasp->name, "ChromeWindow") ||
                            !strcmp(clasp->name, "ModalContentWindow");
    }
    if (mUseContentXBLScope) {
      mUseContentXBLScope = principal && !nsContentUtils::IsSystemPrincipal(principal);
    }

    JSAddonId *addonId = JS::AddonIdOfObject(aGlobal);
    if (gInterpositionMap) {
        if (InterpositionMap::Ptr p = gInterpositionMap->lookup(addonId)) {
            MOZ_RELEASE_ASSERT(nsContentUtils::IsSystemPrincipal(principal));
            mInterposition = p->value();
        }
    }
}

// static
bool
XPCWrappedNativeScope::IsDyingScope(XPCWrappedNativeScope *scope)
{
    for (XPCWrappedNativeScope *cur = gDyingScopes; cur; cur = cur->mNext) {
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
        nsIPrincipal *p = GetPrincipal();
        bool system = nsXPConnect::SecurityManager()->IsSystemPrincipal(p);
        mComponents = system ? new nsXPCComponents(this)
                             : new nsXPCComponentsBase(this);
    }

    RootedValue val(cx);
    xpcObjectHelper helper(mComponents);
    bool ok = XPCConvert::NativeInterface2JSObject(&val, nullptr, helper,
                                                   nullptr, nullptr, false,
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

    RootedId id(aCx, XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_COMPONENTS));
    return JS_DefinePropertyById(aCx, global, id, components,
                                 JSPROP_PERMANENT | JSPROP_READONLY);
}

static bool
CompartmentPerAddon()
{
    static bool initialized = false;
    static bool pref = false;

    if (!initialized) {
        pref = Preferences::GetBool("dom.compartment_per_addon", false) ||
               Preferences::GetBool("browser.tabs.remote.autostart", false);
        initialized = true;
    }

    return pref;
}

JSObject*
XPCWrappedNativeScope::EnsureContentXBLScope(JSContext *cx)
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
    nsIPrincipal *principal = GetPrincipal();
    nsCOMPtr<nsIExpandedPrincipal> ep;
    MOZ_ASSERT(!(ep = do_QueryInterface(principal)));
    nsTArray< nsCOMPtr<nsIPrincipal> > principalAsArray(1);
    principalAsArray.AppendElement(principal);
    ep = new nsExpandedPrincipal(principalAsArray);

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
JSObject *
GetXBLScope(JSContext *cx, JSObject *contentScopeArg)
{
    MOZ_ASSERT(!IsInAddonScope(contentScopeArg));

    JS::RootedObject contentScope(cx, contentScopeArg);
    JSAutoCompartment ac(cx, contentScope);
    JSObject *scope = CompartmentPrivate::Get(contentScope)->scope->EnsureContentXBLScope(cx);
    NS_ENSURE_TRUE(scope, nullptr); // See bug 858642.
    scope = js::UncheckedUnwrap(scope);
    JS::ExposeObjectToActiveJS(scope);
    return scope;
}

JSObject *
GetScopeForXBLExecution(JSContext *cx, HandleObject contentScope, JSAddonId *addonId)
{
    MOZ_RELEASE_ASSERT(!IsInAddonScope(contentScope));

    RootedObject global(cx, js::GetGlobalForObjectCrossCompartment(contentScope));
    if (IsInContentXBLScope(contentScope))
        return global;

    JSAutoCompartment ac(cx, contentScope);
    XPCWrappedNativeScope *nativeScope = CompartmentPrivate::Get(contentScope)->scope;

    RootedObject scope(cx);
    if (nativeScope->UseContentXBLScope())
        scope = nativeScope->EnsureContentXBLScope(cx);
    else if (addonId && CompartmentPerAddon())
        scope = nativeScope->EnsureAddonScope(cx, addonId);
    else
        scope = global;

    NS_ENSURE_TRUE(scope, nullptr); // See bug 858642.
    scope = js::UncheckedUnwrap(scope);
    JS::ExposeObjectToActiveJS(scope);
    return scope;
}

bool
AllowContentXBLScope(JSCompartment *c)
{
  XPCWrappedNativeScope *scope = CompartmentPrivate::Get(c)->scope;
  return scope && scope->AllowContentXBLScope();
}

bool
UseContentXBLScope(JSCompartment *c)
{
  XPCWrappedNativeScope *scope = CompartmentPrivate::Get(c)->scope;
  return scope && scope->UseContentXBLScope();
}

} /* namespace xpc */

JSObject*
XPCWrappedNativeScope::EnsureAddonScope(JSContext *cx, JSAddonId *addonId)
{
    JS::RootedObject global(cx, GetGlobalJSObject());
    MOZ_ASSERT(js::IsObjectInContextCompartment(global, cx));
    MOZ_ASSERT(!mIsContentXBLScope);
    MOZ_ASSERT(!mIsAddonScope);
    MOZ_ASSERT(addonId);
    MOZ_ASSERT(nsContentUtils::IsSystemPrincipal(GetPrincipal()));

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

JSObject *
xpc::GetAddonScope(JSContext *cx, JS::HandleObject contentScope, JSAddonId *addonId)
{
    MOZ_RELEASE_ASSERT(!IsInAddonScope(contentScope));

    if (!addonId || !CompartmentPerAddon()) {
        return js::GetGlobalForObjectCrossCompartment(contentScope);
    }

    JSAutoCompartment ac(cx, contentScope);
    XPCWrappedNativeScope *nativeScope = CompartmentPrivate::Get(contentScope)->scope;
    JSObject *scope = nativeScope->EnsureAddonScope(cx, addonId);
    NS_ENSURE_TRUE(scope, nullptr);

    scope = js::UncheckedUnwrap(scope);
    JS::ExposeObjectToActiveJS(scope);
    return scope;
}

XPCWrappedNativeScope::~XPCWrappedNativeScope()
{
    MOZ_COUNT_DTOR(XPCWrappedNativeScope);

    // We can do additional cleanup assertions here...

    if (mWrappedNativeMap) {
        MOZ_ASSERT(0 == mWrappedNativeMap->Count(), "scope has non-empty map");
        delete mWrappedNativeMap;
    }

    if (mWrappedNativeProtoMap) {
        MOZ_ASSERT(0 == mWrappedNativeProtoMap->Count(), "scope has non-empty map");
        delete mWrappedNativeProtoMap;
    }

    // This should not be necessary, since the Components object should die
    // with the scope but just in case.
    if (mComponents)
        mComponents->mScope = nullptr;

    // XXX we should assert that we are dead or that xpconnect has shutdown
    // XXX might not want to do this at xpconnect shutdown time???
    mComponents = nullptr;

    if (mXrayExpandos.initialized())
        mXrayExpandos.destroy();

    JSRuntime *rt = XPCJSRuntime::Get()->Runtime();
    mContentXBLScope.finalize(rt);
    for (size_t i = 0; i < mAddonScopes.Length(); i++)
        mAddonScopes[i].finalize(rt);
    mGlobalJSObject.finalize(rt);
}

static PLDHashOperator
WrappedNativeJSGCThingTracer(PLDHashTable *table, PLDHashEntryHdr *hdr,
                             uint32_t number, void *arg)
{
    XPCWrappedNative* wrapper = ((Native2WrappedNativeMap::Entry*)hdr)->value;
    if (wrapper->HasExternalReference() && !wrapper->IsWrapperExpired())
        wrapper->TraceSelf((JSTracer *)arg);

    return PL_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::TraceWrappedNativesInAllScopes(JSTracer* trc, XPCJSRuntime* rt)
{
    // Do JS_CallTracer for all wrapped natives with external references, as
    // well as any DOM expando objects.
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(WrappedNativeJSGCThingTracer, trc);
        if (cur->mDOMExpandoSet) {
            for (DOMExpandoSet::Enum e(*cur->mDOMExpandoSet); !e.empty(); e.popFront())
                JS_CallHashSetObjectTracer(trc, e, e.front(), "DOM expando object");
        }
    }
}

static PLDHashOperator
WrappedNativeSuspecter(PLDHashTable *table, PLDHashEntryHdr *hdr,
                       uint32_t number, void *arg)
{
    XPCWrappedNative* wrapper = ((Native2WrappedNativeMap::Entry*)hdr)->value;

    if (wrapper->HasExternalReference()) {
        nsCycleCollectionNoteRootCallback *cb =
            static_cast<nsCycleCollectionNoteRootCallback *>(arg);
        XPCJSRuntime::SuspectWrappedNative(wrapper, *cb);
    }

    return PL_DHASH_NEXT;
}

static void
SuspectDOMExpandos(JSObject *obj, nsCycleCollectionNoteRootCallback &cb)
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
        cur->mWrappedNativeMap->Enumerate(WrappedNativeSuspecter, &cb);
        if (cur->mDOMExpandoSet) {
            for (DOMExpandoSet::Range r = cur->mDOMExpandoSet->all(); !r.empty(); r.popFront())
                SuspectDOMExpandos(r.front(), cb);
        }
    }
}

// static
void
XPCWrappedNativeScope::StartFinalizationPhaseOfGC(JSFreeOp *fop, XPCJSRuntime* rt)
{
    // We are in JSGC_MARK_END and JSGC_FINALIZE_END must always follow it
    // calling FinishedFinalizationPhaseOfGC and clearing gDyingScopes in
    // KillDyingScopes.
    MOZ_ASSERT(!gDyingScopes, "JSGC_MARK_END without JSGC_FINALIZE_END");

    XPCWrappedNativeScope* prev = nullptr;
    XPCWrappedNativeScope* cur = gScopes;

    while (cur) {
        // Sweep waivers.
        if (cur->mWaiverWrapperMap)
            cur->mWaiverWrapperMap->Sweep();

        XPCWrappedNativeScope* next = cur->mNext;

        if (cur->mGlobalJSObject && cur->mGlobalJSObject.isAboutToBeFinalized()) {
            cur->mGlobalJSObject.finalize(fop->runtime());
            // Move this scope from the live list to the dying list.
            if (prev)
                prev->mNext = next;
            else
                gScopes = next;
            cur->mNext = gDyingScopes;
            gDyingScopes = cur;
            cur = nullptr;
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
    KillDyingScopes();
}

static PLDHashOperator
WrappedNativeMarker(PLDHashTable *table, PLDHashEntryHdr *hdr,
                    uint32_t number_t, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->Mark();
    return PL_DHASH_NEXT;
}

// We need to explicitly mark all the protos too because some protos may be
// alive in the hashtable but not currently in use by any wrapper
static PLDHashOperator
WrappedNativeProtoMarker(PLDHashTable *table, PLDHashEntryHdr *hdr,
                         uint32_t number, void *arg)
{
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->Mark();
    return PL_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::MarkAllWrappedNativesAndProtos()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(WrappedNativeMarker, nullptr);
        cur->mWrappedNativeProtoMap->Enumerate(WrappedNativeProtoMarker, nullptr);
    }
}

#ifdef DEBUG
static PLDHashOperator
ASSERT_WrappedNativeSetNotMarked(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                 uint32_t number, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->ASSERT_SetsNotMarked();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
ASSERT_WrappedNativeProtoSetNotMarked(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                      uint32_t number, void *arg)
{
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->ASSERT_SetNotMarked();
    return PL_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::ASSERT_NoInterfaceSetsAreMarked()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext) {
        cur->mWrappedNativeMap->Enumerate(ASSERT_WrappedNativeSetNotMarked, nullptr);
        cur->mWrappedNativeProtoMap->Enumerate(ASSERT_WrappedNativeProtoSetNotMarked, nullptr);
    }
}
#endif

static PLDHashOperator
WrappedNativeTearoffSweeper(PLDHashTable *table, PLDHashEntryHdr *hdr,
                            uint32_t number, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->SweepTearOffs();
    return PL_DHASH_NEXT;
}

// static
void
XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs()
{
    for (XPCWrappedNativeScope* cur = gScopes; cur; cur = cur->mNext)
        cur->mWrappedNativeMap->Enumerate(WrappedNativeTearoffSweeper, nullptr);
}

// static
void
XPCWrappedNativeScope::KillDyingScopes()
{
    XPCWrappedNativeScope* cur = gDyingScopes;
    while (cur) {
        XPCWrappedNativeScope* next = cur->mNext;
        delete cur;
        cur = next;
    }
    gDyingScopes = nullptr;
}

struct ShutdownData
{
    ShutdownData()
        : wrapperCount(0),
          protoCount(0) {}
    int wrapperCount;
    int protoCount;
};

static PLDHashOperator
WrappedNativeShutdownEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                uint32_t number, void *arg)
{
    ShutdownData* data = (ShutdownData*) arg;
    XPCWrappedNative* wrapper = ((Native2WrappedNativeMap::Entry*)hdr)->value;

    if (wrapper->IsValid()) {
        wrapper->SystemIsBeingShutDown();
        data->wrapperCount++;
    }
    return PL_DHASH_REMOVE;
}

static PLDHashOperator
WrappedNativeProtoShutdownEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                     uint32_t number, void *arg)
{
    ShutdownData* data = (ShutdownData*) arg;
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->
        SystemIsBeingShutDown();
    data->protoCount++;
    return PL_DHASH_REMOVE;
}

//static
void
XPCWrappedNativeScope::SystemIsBeingShutDown()
{
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
        cur->mWrappedNativeProtoMap->
                Enumerate(WrappedNativeProtoShutdownEnumerator,  &data);
        cur->mWrappedNativeMap->
                Enumerate(WrappedNativeShutdownEnumerator,  &data);
    }

    // Now it is safe to kill all the scopes.
    KillDyingScopes();

    if (gInterpositionMap)
        delete gInterpositionMap;
}


/***************************************************************************/

JSObject *
XPCWrappedNativeScope::GetExpandoChain(HandleObject target)
{
    MOZ_ASSERT(ObjectScope(target) == this);
    if (!mXrayExpandos.initialized())
        return nullptr;
    return mXrayExpandos.lookup(target);
}

bool
XPCWrappedNativeScope::SetExpandoChain(JSContext *cx, HandleObject target,
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
XPCWrappedNativeScope::SetAddonInterposition(JSAddonId *addonId,
                                             nsIAddonInterposition *interp)
{
    if (!gInterpositionMap) {
        gInterpositionMap = new InterpositionMap();
        gInterpositionMap->init();
    }
    if (interp) {
        return gInterpositionMap->put(addonId, interp);
    } else {
        gInterpositionMap->remove(addonId);
        return true;
    }
}

nsCOMPtr<nsIAddonInterposition>
XPCWrappedNativeScope::GetInterposition()
{
    return mInterposition;
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

#ifdef DEBUG
static PLDHashOperator
WrappedNativeMapDumpEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                               uint32_t number, void *arg)
{
    ((Native2WrappedNativeMap::Entry*)hdr)->value->DebugDump(*(int16_t*)arg);
    return PL_DHASH_NEXT;
}
static PLDHashOperator
WrappedNativeProtoMapDumpEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    uint32_t number, void *arg)
{
    ((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value->DebugDump(*(int16_t*)arg);
    return PL_DHASH_NEXT;
}
#endif

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
    XPC_LOG_OUTDENT();
#endif
}

void
XPCWrappedNativeScope::AddSizeOfAllScopesIncludingThis(ScopeSizeInfo* scopeSizeInfo)
{
    for (XPCWrappedNativeScope *cur = gScopes; cur; cur = cur->mNext)
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
