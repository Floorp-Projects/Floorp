/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Per JSRuntime object */

#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"

#include "xpcprivate.h"
#include "xpcpublic.h"
#include "XPCWrapper.h"
#include "XPCJSMemoryReporter.h"
#include "WrapperFactory.h"
#include "mozJSComponentLoader.h"
#include "nsAutoPtr.h"
#include "nsNetUtil.h"

#include "nsIMemoryInfoDumper.h"
#include "nsIMemoryReporter.h"
#include "nsIObserverService.h"
#include "nsIDebug2.h"
#include "nsIDocShell.h"
#include "nsIRunnable.h"
#include "amIAddonManager.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ScriptSettings.h"

#include "nsContentUtils.h"
#include "nsCCUncollectableMarker.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollector.h"
#include "jsapi.h"
#include "jsprf.h"
#include "js/MemoryMetrics.h"
#include "mozilla/dom/GeneratedAtomList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ProcessHangMonitor.h"
#include "mozilla/Sprintf.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "AccessCheck.h"
#include "nsGlobalWindow.h"
#include "nsAboutProtocolUtils.h"

#include "GeckoProfiler.h"
#include "nsIInputStream.h"
#include "nsIXULRuntime.h"
#include "nsJSPrincipals.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

using namespace mozilla;
using namespace xpc;
using namespace JS;
using mozilla::dom::PerThreadAtomCache;
using mozilla::dom::AutoEntryScript;

/***************************************************************************/

const char* const XPCJSRuntime::mStrings[] = {
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
    "eval",                 // IDX_EVAL
    "controllers",          // IDX_CONTROLLERS
    "Controllers",          // IDX_CONTROLLERS_CLASS
    "realFrameElement",     // IDX_REALFRAMEELEMENT
    "length",               // IDX_LENGTH
    "name",                 // IDX_NAME
    "undefined",            // IDX_UNDEFINED
    "",                     // IDX_EMPTYSTRING
    "fileName",             // IDX_FILENAME
    "lineNumber",           // IDX_LINENUMBER
    "columnNumber",         // IDX_COLUMNNUMBER
    "stack",                // IDX_STACK
    "message",              // IDX_MESSAGE
    "lastIndex"             // IDX_LASTINDEX
};

/***************************************************************************/

// *Some* NativeSets are referenced from mClassInfo2NativeSetMap.
// *All* NativeSets are referenced from mNativeSetMap.
// So, in mClassInfo2NativeSetMap we just clear references to the unmarked.
// In mNativeSetMap we clear the references to the unmarked *and* delete them.

class AsyncFreeSnowWhite : public Runnable
{
public:
  NS_IMETHOD Run() override
  {
      TimeStamp start = TimeStamp::Now();
      bool hadSnowWhiteObjects = nsCycleCollector_doDeferredDeletion();
      Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_ASYNC_SNOW_WHITE_FREEING,
                            uint32_t((TimeStamp::Now() - start).ToMilliseconds()));
      if (hadSnowWhiteObjects && !mContinuation) {
          mContinuation = true;
          if (NS_FAILED(Dispatch())) {
              mActive = false;
          }
      } else {
          mActive = false;
      }
      return NS_OK;
  }

  nsresult Dispatch()
  {
      nsCOMPtr<nsIRunnable> self(this);
      return NS_IdleDispatchToCurrentThread(self.forget(), 2500);
  }

  void Start(bool aContinuation = false, bool aPurge = false)
  {
      if (mContinuation) {
          mContinuation = aContinuation;
      }
      mPurge = aPurge;
      if (!mActive && NS_SUCCEEDED(Dispatch())) {
          mActive = true;
      }
  }

  AsyncFreeSnowWhite()
    : Runnable("AsyncFreeSnowWhite")
    , mContinuation(false)
    , mActive(false)
    , mPurge(false) {}

public:
  bool mContinuation;
  bool mActive;
  bool mPurge;
};

namespace xpc {

CompartmentPrivate::CompartmentPrivate(JSCompartment* c)
    : wantXrays(false)
    , allowWaivers(true)
    , writeToGlobalPrototype(false)
    , skipWriteToGlobalPrototype(false)
    , isWebExtensionContentScript(false)
    , waiveInterposition(false)
    , allowCPOWs(false)
    , universalXPConnectEnabled(false)
    , forcePermissiveCOWs(false)
    , wasNuked(false)
    , scriptability(c)
    , scope(nullptr)
    , mWrappedJSMap(JSObject2WrappedJSMap::newMap(XPC_JS_MAP_LENGTH))
{
    MOZ_COUNT_CTOR(xpc::CompartmentPrivate);
    mozilla::PodArrayZero(wrapperDenialWarnings);
}

CompartmentPrivate::~CompartmentPrivate()
{
    MOZ_COUNT_DTOR(xpc::CompartmentPrivate);
    mWrappedJSMap->ShutdownMarker();
    delete mWrappedJSMap;
}

static bool
TryParseLocationURICandidate(const nsACString& uristr,
                             CompartmentPrivate::LocationHint aLocationHint,
                             nsIURI** aURI)
{
    static NS_NAMED_LITERAL_CSTRING(kGRE, "resource://gre/");
    static NS_NAMED_LITERAL_CSTRING(kToolkit, "chrome://global/");
    static NS_NAMED_LITERAL_CSTRING(kBrowser, "chrome://browser/");

    if (aLocationHint == CompartmentPrivate::LocationHintAddon) {
        // Blacklist some known locations which are clearly not add-on related.
        if (StringBeginsWith(uristr, kGRE) ||
            StringBeginsWith(uristr, kToolkit) ||
            StringBeginsWith(uristr, kBrowser))
            return false;

        // -- GROSS HACK ALERT --
        // The Yandex Elements 8.10.2 extension implements its own "xb://" URL
        // scheme. If we call NS_NewURI() on an "xb://..." URL, we'll end up
        // calling into the extension's own JS-implemented nsIProtocolHandler
        // object, which we can't allow while we're iterating over the JS heap.
        // So just skip any such URL.
        // -- GROSS HACK ALERT --
        if (StringBeginsWith(uristr, NS_LITERAL_CSTRING("xb")))
            return false;
    }

    nsCOMPtr<nsIURI> uri;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), uristr)))
        return false;

    nsAutoCString scheme;
    if (NS_FAILED(uri->GetScheme(scheme)))
        return false;

    // Cannot really map data: and blob:.
    // Also, data: URIs are pretty memory hungry, which is kinda bad
    // for memory reporter use.
    if (scheme.EqualsLiteral("data") || scheme.EqualsLiteral("blob"))
        return false;

    uri.forget(aURI);
    return true;
}

bool CompartmentPrivate::TryParseLocationURI(CompartmentPrivate::LocationHint aLocationHint,
                                             nsIURI** aURI)
{
    if (!aURI)
        return false;

    // Need to parse the URI.
    if (location.IsEmpty())
        return false;

    // Handle Sandbox location strings.
    // A sandbox string looks like this:
    // <sandboxName> (from: <js-stack-frame-filename>:<lineno>)
    // where <sandboxName> is user-provided via Cu.Sandbox()
    // and <js-stack-frame-filename> and <lineno> is the stack frame location
    // from where Cu.Sandbox was called.
    // <js-stack-frame-filename> furthermore is "free form", often using a
    // "uri -> uri -> ..." chain. The following code will and must handle this
    // common case.
    // It should be noted that other parts of the code may already rely on the
    // "format" of these strings, such as the add-on SDK.

    static const nsDependentCString from("(from: ");
    static const nsDependentCString arrow(" -> ");
    static const size_t fromLength = from.Length();
    static const size_t arrowLength = arrow.Length();

    // See: XPCComponents.cpp#AssembleSandboxMemoryReporterName
    int32_t idx = location.Find(from);
    if (idx < 0)
        return TryParseLocationURICandidate(location, aLocationHint, aURI);


    // When parsing we're looking for the right-most URI. This URI may be in
    // <sandboxName>, so we try this first.
    if (TryParseLocationURICandidate(Substring(location, 0, idx), aLocationHint,
                                     aURI))
        return true;

    // Not in <sandboxName> so we need to inspect <js-stack-frame-filename> and
    // the chain that is potentially contained within and grab the rightmost
    // item that is actually a URI.

    // First, hack off the :<lineno>) part as well
    int32_t ridx = location.RFind(NS_LITERAL_CSTRING(":"));
    nsAutoCString chain(Substring(location, idx + fromLength,
                                  ridx - idx - fromLength));

    // Loop over the "->" chain. This loop also works for non-chains, or more
    // correctly chains with only one item.
    for (;;) {
        idx = chain.RFind(arrow);
        if (idx < 0) {
            // This is the last chain item. Try to parse what is left.
            return TryParseLocationURICandidate(chain, aLocationHint, aURI);
        }

        // Try to parse current chain item
        if (TryParseLocationURICandidate(Substring(chain, idx + arrowLength),
                                         aLocationHint, aURI))
            return true;

        // Current chain item couldn't be parsed.
        // Strip current item and continue.
        chain = Substring(chain, 0, idx);
    }

    MOZ_CRASH("Chain parser loop does not terminate");
}

static bool
PrincipalImmuneToScriptPolicy(nsIPrincipal* aPrincipal)
{
    // System principal gets a free pass.
    if (nsXPConnect::SecurityManager()->IsSystemPrincipal(aPrincipal))
        return true;

    // ExpandedPrincipal gets a free pass.
    nsCOMPtr<nsIExpandedPrincipal> ep = do_QueryInterface(aPrincipal);
    if (ep)
        return true;

    // Check whether our URI is an "about:" URI that allows scripts.  If it is,
    // we need to allow JS to run.
    nsCOMPtr<nsIURI> principalURI;
    aPrincipal->GetURI(getter_AddRefs(principalURI));
    MOZ_ASSERT(principalURI);

    // WebExtension principals gets a free pass.
    nsString addonId;
    aPrincipal->GetAddonId(addonId);
    bool isWebExtension = !addonId.IsEmpty();
    if (isWebExtension) {
        return true;
    }

    bool isAbout;
    nsresult rv = principalURI->SchemeIs("about", &isAbout);
    if (NS_SUCCEEDED(rv) && isAbout) {
        nsCOMPtr<nsIAboutModule> module;
        rv = NS_GetAboutModule(principalURI, getter_AddRefs(module));
        if (NS_SUCCEEDED(rv)) {
            uint32_t flags;
            rv = module->GetURIFlags(principalURI, &flags);
            if (NS_SUCCEEDED(rv) &&
                (flags & nsIAboutModule::ALLOW_SCRIPT)) {
                return true;
            }
        }
    }

    return false;
}

Scriptability::Scriptability(JSCompartment* c) : mScriptBlocks(0)
                                               , mDocShellAllowsScript(true)
                                               , mScriptBlockedByPolicy(false)
{
    nsIPrincipal* prin = nsJSPrincipals::get(JS_GetCompartmentPrincipals(c));
    mImmuneToScriptPolicy = PrincipalImmuneToScriptPolicy(prin);

    // If we're not immune, we should have a real principal with a codebase URI.
    // Check the URI against the new-style domain policy.
    if (!mImmuneToScriptPolicy) {
        nsCOMPtr<nsIURI> codebase;
        nsresult rv = prin->GetURI(getter_AddRefs(codebase));
        bool policyAllows;
        if (NS_SUCCEEDED(rv) && codebase &&
            NS_SUCCEEDED(nsXPConnect::SecurityManager()->PolicyAllowsScript(codebase, &policyAllows)))
        {
            mScriptBlockedByPolicy = !policyAllows;
        } else {
            // Something went wrong - be safe and block script.
            mScriptBlockedByPolicy = true;
        }
    }
}

bool
Scriptability::Allowed()
{
    return mDocShellAllowsScript && !mScriptBlockedByPolicy &&
           mScriptBlocks == 0;
}

bool
Scriptability::IsImmuneToScriptPolicy()
{
    return mImmuneToScriptPolicy;
}

void
Scriptability::Block()
{
    ++mScriptBlocks;
}

void
Scriptability::Unblock()
{
    MOZ_ASSERT(mScriptBlocks > 0);
    --mScriptBlocks;
}

void
Scriptability::SetDocShellAllowsScript(bool aAllowed)
{
    mDocShellAllowsScript = aAllowed || mImmuneToScriptPolicy;
}

/* static */
Scriptability&
Scriptability::Get(JSObject* aScope)
{
    return CompartmentPrivate::Get(aScope)->scriptability;
}

bool
IsContentXBLScope(JSCompartment* compartment)
{
    // We always eagerly create compartment privates for XBL scopes.
    CompartmentPrivate* priv = CompartmentPrivate::Get(compartment);
    if (!priv || !priv->scope)
        return false;
    return priv->scope->IsContentXBLScope();
}

bool
IsInContentXBLScope(JSObject* obj)
{
    return IsContentXBLScope(js::GetObjectCompartment(obj));
}

bool
IsInAddonScope(JSObject* obj)
{
    return ObjectScope(obj)->IsAddonScope();
}

bool
IsUniversalXPConnectEnabled(JSCompartment* compartment)
{
    CompartmentPrivate* priv = CompartmentPrivate::Get(compartment);
    if (!priv)
        return false;
    return priv->universalXPConnectEnabled;
}

bool
IsUniversalXPConnectEnabled(JSContext* cx)
{
    JSCompartment* compartment = js::GetContextCompartment(cx);
    if (!compartment)
        return false;
    return IsUniversalXPConnectEnabled(compartment);
}

bool
EnableUniversalXPConnect(JSContext* cx)
{
    JSCompartment* compartment = js::GetContextCompartment(cx);
    if (!compartment)
        return true;
    // Never set universalXPConnectEnabled on a chrome compartment - it confuses
    // the security wrapping code.
    if (AccessCheck::isChrome(compartment))
        return true;
    CompartmentPrivate* priv = CompartmentPrivate::Get(compartment);
    if (!priv)
        return true;
    if (priv->universalXPConnectEnabled)
        return true;
    priv->universalXPConnectEnabled = true;

    // Recompute all the cross-compartment wrappers leaving the newly-privileged
    // compartment.
    bool ok = js::RecomputeWrappers(cx, js::SingleCompartment(compartment),
                                    js::AllCompartments());
    NS_ENSURE_TRUE(ok, false);

    // The Components object normally isn't defined for unprivileged web content,
    // but we define it when UniversalXPConnect is enabled to support legacy
    // tests.
    XPCWrappedNativeScope* scope = priv->scope;
    if (!scope)
        return true;
    scope->ForcePrivilegedComponents();
    return scope->AttachComponentsObject(cx);
}

JSObject*
UnprivilegedJunkScope()
{
    return XPCJSRuntime::Get()->UnprivilegedJunkScope();
}

JSObject*
PrivilegedJunkScope()
{
    return XPCJSRuntime::Get()->PrivilegedJunkScope();
}

JSObject*
CompilationScope()
{
    return XPCJSRuntime::Get()->CompilationScope();
}

nsGlobalWindow*
WindowOrNull(JSObject* aObj)
{
    MOZ_ASSERT(aObj);
    MOZ_ASSERT(!js::IsWrapper(aObj));

    nsGlobalWindow* win = nullptr;
    UNWRAP_NON_WRAPPER_OBJECT(Window, aObj, win);
    return win;
}

nsGlobalWindow*
WindowGlobalOrNull(JSObject* aObj)
{
    MOZ_ASSERT(aObj);
    JSObject* glob = js::GetGlobalForObjectCrossCompartment(aObj);

    return WindowOrNull(glob);
}

nsGlobalWindow*
AddonWindowOrNull(JSObject* aObj)
{
    if (!IsInAddonScope(aObj))
        return nullptr;

    JSObject* global = js::GetGlobalForObjectCrossCompartment(aObj);
    JSObject* proto = js::GetPrototypeNoProxy(global);

    // Addons could theoretically change the prototype of the addon scope, but
    // we pretty much just want to crash if that happens so that we find out
    // about it and get them to change their code.
    MOZ_RELEASE_ASSERT(js::IsCrossCompartmentWrapper(proto) ||
                       xpc::IsSandboxPrototypeProxy(proto));
    JSObject* mainGlobal = js::UncheckedUnwrap(proto, /* stopAtWindowProxy = */ false);
    MOZ_RELEASE_ASSERT(JS_IsGlobalObject(mainGlobal));

    return WindowOrNull(mainGlobal);
}

nsGlobalWindow*
CurrentWindowOrNull(JSContext* cx)
{
    JSObject* glob = JS::CurrentGlobalOrNull(cx);
    return glob ? WindowOrNull(glob) : nullptr;
}

// Nukes all wrappers into or out of the given compartment, and prevents new
// wrappers from being created. Additionally marks the compartment as
// unscriptable after wrappers have been nuked.
//
// Note: This should *only* be called for browser or extension compartments.
// Wrappers between web compartments must never be cut in web-observable
// ways.
void
NukeAllWrappersForCompartment(JSContext* cx, JSCompartment* compartment,
                              js::NukeReferencesToWindow nukeReferencesToWindow)
{
    // First, nuke all wrappers into or out of the target compartment. Once
    // the compartment is marked as nuked, WrapperFactory will refuse to
    // create new live wrappers for it, in either direction. This means that
    // we need to be sure that we don't have any existing cross-compartment
    // wrappers which may be replaced with dead wrappers during unrelated
    // wrapper recomputation *before* we set that bit.
    js::NukeCrossCompartmentWrappers(cx, js::AllCompartments(), compartment,
                                     nukeReferencesToWindow,
                                     js::NukeAllReferences);

    // At this point, we should cross-compartment wrappers for the nuked
    // compartment. Set the wasNuked bit so WrapperFactory will return a
    // DeadObjectProxy when asked to create a new wrapper for it, and mark as
    // unscriptable.
    auto compartmentPrivate = xpc::CompartmentPrivate::Get(compartment);
    compartmentPrivate->wasNuked = true;
    compartmentPrivate->scriptability.Block();
}

} // namespace xpc

static void
CompartmentDestroyedCallback(JSFreeOp* fop, JSCompartment* compartment)
{
    // NB - This callback may be called in JS_DestroyContext, which happens
    // after the XPCJSRuntime has been torn down.

    // Get the current compartment private into an AutoPtr (which will do the
    // cleanup for us), and null out the private (which may already be null).
    nsAutoPtr<CompartmentPrivate> priv(CompartmentPrivate::Get(compartment));
    JS_SetCompartmentPrivate(compartment, nullptr);
}

static size_t
CompartmentSizeOfIncludingThisCallback(MallocSizeOf mallocSizeOf, JSCompartment* compartment)
{
    CompartmentPrivate* priv = CompartmentPrivate::Get(compartment);
    return priv ? priv->SizeOfIncludingThis(mallocSizeOf) : 0;
}

/*
 * Return true if there exists a non-system inner window which is a current
 * inner window and whose reflector is gray.  We don't merge system
 * compartments, so we don't use them to trigger merging CCs.
 */
bool XPCJSRuntime::UsefulToMergeZones() const
{
    MOZ_ASSERT(NS_IsMainThread());

    // Turns out, actually making this return true often enough makes Windows
    // mochitest-gl OOM a lot.  Need to figure out what's going on there; see
    // bug 1277036.

    return false;
}

void XPCJSRuntime::TraceNativeBlackRoots(JSTracer* trc)
{
    for (CycleCollectedJSContext* ccx : Contexts()) {
        auto* cx = static_cast<const XPCJSContext*>(ccx);
        if (AutoMarkingPtr* roots = cx->mAutoRoots)
            roots->TraceJSAll(trc);
    }

    JSContext* cx = XPCJSContext::Get()->Context();
    dom::TraceBlackJS(trc, JS_GetGCParameter(cx, JSGC_NUMBER),
                      nsXPConnect::XPConnect()->IsShuttingDown());
}

void XPCJSRuntime::TraceAdditionalNativeGrayRoots(JSTracer* trc)
{
    XPCWrappedNativeScope::TraceWrappedNativesInAllScopes(trc);

    for (XPCRootSetElem* e = mVariantRoots; e ; e = e->GetNextRoot())
        static_cast<XPCTraceableVariant*>(e)->TraceJS(trc);

    for (XPCRootSetElem* e = mWrappedJSRoots; e ; e = e->GetNextRoot())
        static_cast<nsXPCWrappedJS*>(e)->TraceJS(trc);
}

void
XPCJSRuntime::TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& cb)
{
    XPCWrappedNativeScope::SuspectAllWrappers(cb);

    for (XPCRootSetElem* e = mVariantRoots; e ; e = e->GetNextRoot()) {
        XPCTraceableVariant* v = static_cast<XPCTraceableVariant*>(e);
        if (nsCCUncollectableMarker::InGeneration(cb,
                                                  v->CCGeneration())) {
           JS::Value val = v->GetJSValPreserveColor();
           if (val.isObject() && !JS::ObjectIsMarkedGray(&val.toObject()))
               continue;
        }
        cb.NoteXPCOMRoot(v);
    }

    for (XPCRootSetElem* e = mWrappedJSRoots; e ; e = e->GetNextRoot()) {
        cb.NoteXPCOMRoot(ToSupports(static_cast<nsXPCWrappedJS*>(e)));
    }
}

void
XPCJSRuntime::UnmarkSkippableJSHolders()
{
    CycleCollectedJSRuntime::UnmarkSkippableJSHolders();
}

void
XPCJSRuntime::PrepareForForgetSkippable()
{
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->NotifyObservers(nullptr, "cycle-collector-forget-skippable", nullptr);
    }
}

void
XPCJSRuntime::BeginCycleCollectionCallback()
{
    nsJSContext::BeginCycleCollectionCallback();

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->NotifyObservers(nullptr, "cycle-collector-begin", nullptr);
    }
}

void
XPCJSRuntime::EndCycleCollectionCallback(CycleCollectorResults& aResults)
{
    nsJSContext::EndCycleCollectionCallback(aResults);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
        obs->NotifyObservers(nullptr, "cycle-collector-end", nullptr);
    }
}

void
XPCJSRuntime::DispatchDeferredDeletion(bool aContinuation, bool aPurge)
{
    mAsyncSnowWhiteFreer->Start(aContinuation, aPurge);
}

void
xpc_UnmarkSkippableJSHolders()
{
    if (nsXPConnect::GetRuntimeInstance()) {
        nsXPConnect::GetRuntimeInstance()->UnmarkSkippableJSHolders();
    }
}

/* static */ void
XPCJSRuntime::GCSliceCallback(JSContext* cx,
                              JS::GCProgress progress,
                              const JS::GCDescription& desc)
{
    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if (!self)
        return;

#ifdef MOZ_CRASHREPORTER
    CrashReporter::SetGarbageCollecting(progress == JS::GC_CYCLE_BEGIN);
#endif

    if (self->mPrevGCSliceCallback)
        (*self->mPrevGCSliceCallback)(cx, progress, desc);
}

/* static */ void
XPCJSRuntime::DoCycleCollectionCallback(JSContext* cx)
{
    // The GC has detected that a CC at this point would collect a tremendous
    // amount of garbage that is being revivified unnecessarily.
    NS_DispatchToCurrentThread(
      NS_NewRunnableFunction("XPCJSRuntime::DoCycleCollectionCallback",
                             []() { nsJSContext::CycleCollectNow(nullptr); }));

    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if (!self)
        return;

    if (self->mPrevDoCycleCollectionCallback)
        (*self->mPrevDoCycleCollectionCallback)(cx);
}

void
XPCJSRuntime::CustomGCCallback(JSGCStatus status)
{
    nsTArray<xpcGCCallback> callbacks(extraGCCallbacks);
    for (uint32_t i = 0; i < callbacks.Length(); ++i)
        callbacks[i](status);
}

/* static */ void
XPCJSRuntime::FinalizeCallback(JSFreeOp* fop,
                               JSFinalizeStatus status,
                               bool isZoneGC,
                               void* data)
{
    XPCJSRuntime* self = nsXPConnect::GetRuntimeInstance();
    if (!self)
        return;

    switch (status) {
        case JSFINALIZE_GROUP_PREPARE:
        {
            MOZ_ASSERT(!self->mDoingFinalization, "bad state");

            MOZ_ASSERT(!self->mGCIsRunning, "bad state");
            self->mGCIsRunning = true;

            self->mDoingFinalization = true;

            break;
        }
        case JSFINALIZE_GROUP_START:
        {
            MOZ_ASSERT(self->mDoingFinalization, "bad state");

            MOZ_ASSERT(self->mGCIsRunning, "bad state");
            self->mGCIsRunning = false;

            break;
        }
        case JSFINALIZE_GROUP_END:
        {
            // Sweep scopes needing cleanup
            XPCWrappedNativeScope::KillDyingScopes();

            MOZ_ASSERT(self->mDoingFinalization, "bad state");
            self->mDoingFinalization = false;

            break;
        }
        case JSFINALIZE_COLLECTION_END:
        {
            MOZ_ASSERT(!self->mGCIsRunning, "bad state");
            self->mGCIsRunning = true;

            for (CycleCollectedJSContext* ccx : self->Contexts()) {
                auto* cx = static_cast<const XPCJSContext*>(ccx);
                if (AutoMarkingPtr* roots = cx->mAutoRoots)
                    roots->MarkAfterJSFinalizeAll();

                // Now we are going to recycle any unused WrappedNativeTearoffs.
                // We do this by iterating all the live callcontexts
                // and marking the tearoffs in use. And then we
                // iterate over all the WrappedNative wrappers and sweep their
                // tearoffs.
                //
                // This allows us to perhaps minimize the growth of the
                // tearoffs. And also makes us not hold references to interfaces
                // on our wrapped natives that we are not actually using.
                //
                // XXX We may decide to not do this on *every* gc cycle.

                XPCCallContext* ccxp = cx->GetCallContext();
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

            XPCWrappedNativeScope::SweepAllWrappedNativeTearOffs();

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

            for (auto i = self->mDyingWrappedNativeProtoMap->Iter(); !i.Done(); i.Next()) {
                auto entry = static_cast<XPCWrappedNativeProtoMap::Entry*>(i.Get());
                delete static_cast<const XPCWrappedNativeProto*>(entry->key);
                i.Remove();
            }

            MOZ_ASSERT(self->mGCIsRunning, "bad state");
            self->mGCIsRunning = false;

            break;
        }
    }
}

/* static */ void
XPCJSRuntime::WeakPointerZonesCallback(JSContext* cx, void* data)
{
    // Called before each sweeping slice -- after processing any final marking
    // triggered by barriers -- to clear out any references to things that are
    // about to be finalized and update any pointers to moved GC things.
    XPCJSRuntime* self = static_cast<XPCJSRuntime*>(data);

    self->mWrappedJSMap->UpdateWeakPointersAfterGC();

    XPCWrappedNativeScope::UpdateWeakPointersInAllScopesAfterGC();
}

/* static */ void
XPCJSRuntime::WeakPointerCompartmentCallback(JSContext* cx, JSCompartment* comp, void* data)
{
    // Called immediately after the ZoneGroup weak pointer callback, but only
    // once for each compartment that is being swept.
    CompartmentPrivate* xpcComp = CompartmentPrivate::Get(comp);
    if (xpcComp)
        xpcComp->UpdateWeakPointersAfterGC();
}

void
CompartmentPrivate::UpdateWeakPointersAfterGC()
{
    mWrappedJSMap->UpdateWeakPointersAfterGC();
}

void
XPCJSRuntime::CustomOutOfMemoryCallback()
{
    if (!Preferences::GetBool("memory.dump_reports_on_oom")) {
        return;
    }

    nsCOMPtr<nsIMemoryInfoDumper> dumper =
        do_GetService("@mozilla.org/memory-info-dumper;1");
    if (!dumper) {
        return;
    }

    // If this fails, it fails silently.
    dumper->DumpMemoryInfoToTempDir(NS_LITERAL_STRING("due-to-JS-OOM"),
                                    /* anonymize = */ false,
                                    /* minimizeMemoryUsage = */ false);
}

void
XPCJSRuntime::OnLargeAllocationFailure()
{
    CycleCollectedJSRuntime::SetLargeAllocationFailure(OOMState::Reporting);

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
        os->NotifyObservers(nullptr, "memory-pressure", u"heap-minimize");
    }

    CycleCollectedJSRuntime::SetLargeAllocationFailure(OOMState::Reported);
}

class LargeAllocationFailureRunnable final : public Runnable
{
    Mutex mMutex;
    CondVar mCondVar;
    bool mWaiting;

    virtual ~LargeAllocationFailureRunnable()
    {
        MOZ_ASSERT(!mWaiting);
    }

  protected:
    NS_IMETHOD Run() override
    {
        MOZ_ASSERT(NS_IsMainThread());

        XPCJSRuntime::Get()->OnLargeAllocationFailure();

        MutexAutoLock lock(mMutex);
        MOZ_ASSERT(mWaiting);

        mWaiting = false;
        mCondVar.Notify();
        return NS_OK;
    }

  public:
    LargeAllocationFailureRunnable()
      : mozilla::Runnable("LargeAllocationFailureRunnable")
      , mMutex("LargeAllocationFailureRunnable::mMutex")
      , mCondVar(mMutex, "LargeAllocationFailureRunnable::mCondVar")
      , mWaiting(true)
    {
        MOZ_ASSERT(!NS_IsMainThread());
    }

    void BlockUntilDone()
    {
        MOZ_ASSERT(!NS_IsMainThread());

        MutexAutoLock lock(mMutex);
        while (mWaiting) {
            mCondVar.Wait();
        }
    }
};

static void
OnLargeAllocationFailureCallback()
{
    // This callback can be called from any thread, including internal JS helper
    // and DOM worker threads. We need to send the low-memory event via the
    // observer service which can only be called on the main thread, so proxy to
    // the main thread if we're not there already. The purpose of this callback
    // is to synchronously free some memory so the caller can retry a failed
    // allocation, so block on the completion.

    if (NS_IsMainThread()) {
        XPCJSRuntime::Get()->OnLargeAllocationFailure();
        return;
    }

    RefPtr<LargeAllocationFailureRunnable> r = new LargeAllocationFailureRunnable;
    if (NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(r)))) {
        return;
    }

    r->BlockUntilDone();
}

size_t
XPCJSRuntime::SizeOfIncludingThis(MallocSizeOf mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    n += mWrappedJSMap->SizeOfIncludingThis(mallocSizeOf);
    n += mIID2NativeInterfaceMap->SizeOfIncludingThis(mallocSizeOf);
    n += mClassInfo2NativeSetMap->ShallowSizeOfIncludingThis(mallocSizeOf);
    n += mNativeSetMap->SizeOfIncludingThis(mallocSizeOf);

    n += CycleCollectedJSRuntime::SizeOfExcludingThis(mallocSizeOf);

    // There are other XPCJSRuntime members that could be measured; the above
    // ones have been seen by DMD to be worth measuring.  More stuff may be
    // added later.

    return n;
}

size_t
CompartmentPrivate::SizeOfIncludingThis(MallocSizeOf mallocSizeOf)
{
    size_t n = mallocSizeOf(this);
    n += mWrappedJSMap->SizeOfIncludingThis(mallocSizeOf);
    n += mWrappedJSMap->SizeOfWrappedJS(mallocSizeOf);
    return n;
}

/***************************************************************************/

void
XPCJSRuntime::Shutdown(JSContext* cx)
{
    // This destructor runs before ~CycleCollectedJSContext, which does the
    // actual JS_DestroyContext() call. But destroying the context triggers
    // one final GC, which can call back into the context with various
    // callbacks if we aren't careful. Null out the relevant callbacks.
    JS_RemoveFinalizeCallback(cx, FinalizeCallback);
    JS_RemoveWeakPointerZonesCallback(cx, WeakPointerZonesCallback);
    JS_RemoveWeakPointerCompartmentCallback(cx, WeakPointerCompartmentCallback);

    JS::SetGCSliceCallback(cx, mPrevGCSliceCallback);

    // We don't want to track wrapped JS roots after this point since we're
    // making them !IsValid anyway through SystemIsBeingShutDown.
    mWrappedJSRoots = nullptr;

    // clean up and destroy maps...
    mWrappedJSMap->ShutdownMarker();
    delete mWrappedJSMap;
    mWrappedJSMap = nullptr;

    delete mWrappedJSClassMap;
    mWrappedJSClassMap = nullptr;

    delete mIID2NativeInterfaceMap;
    mIID2NativeInterfaceMap = nullptr;

    delete mClassInfo2NativeSetMap;
    mClassInfo2NativeSetMap = nullptr;

    delete mNativeSetMap;
    mNativeSetMap = nullptr;

    delete mThisTranslatorMap;
    mThisTranslatorMap = nullptr;

    delete mDyingWrappedNativeProtoMap;
    mDyingWrappedNativeProtoMap = nullptr;

    CycleCollectedJSRuntime::Shutdown(cx);
}

XPCJSRuntime::~XPCJSRuntime()
{
    MOZ_COUNT_DTOR_INHERITED(XPCJSRuntime, CycleCollectedJSRuntime);
}

// If |*anonymizeID| is non-zero and this is a user compartment, the name will
// be anonymized.
static void
GetCompartmentName(JSCompartment* c, nsCString& name, int* anonymizeID,
                   bool replaceSlashes)
{
    if (js::IsAtomsCompartment(c)) {
        name.AssignLiteral("atoms");
    } else if (*anonymizeID && !js::IsSystemCompartment(c)) {
        name.AppendPrintf("<anonymized-%d>", *anonymizeID);
        *anonymizeID += 1;
    } else if (JSPrincipals* principals = JS_GetCompartmentPrincipals(c)) {
        nsresult rv = nsJSPrincipals::get(principals)->GetScriptLocation(name);
        if (NS_FAILED(rv)) {
            name.AssignLiteral("(unknown)");
        }

        // If the compartment's location (name) differs from the principal's
        // script location, append the compartment's location to allow
        // differentiation of multiple compartments owned by the same principal
        // (e.g. components owned by the system or null principal).
        CompartmentPrivate* compartmentPrivate = CompartmentPrivate::Get(c);
        if (compartmentPrivate) {
            const nsACString& location = compartmentPrivate->GetLocation();
            if (!location.IsEmpty() && !location.Equals(name)) {
                name.AppendLiteral(", ");
                name.Append(location);
            }
        }

        if (*anonymizeID) {
            // We might have a file:// URL that includes a path from the local
            // filesystem, which should be omitted if we're anonymizing.
            static const char* filePrefix = "file://";
            int filePos = name.Find(filePrefix);
            if (filePos >= 0) {
                int pathPos = filePos + strlen(filePrefix);
                int lastSlashPos = -1;
                for (int i = pathPos; i < int(name.Length()); i++) {
                    if (name[i] == '/' || name[i] == '\\') {
                        lastSlashPos = i;
                    }
                }
                if (lastSlashPos != -1) {
                    name.ReplaceASCII(pathPos, lastSlashPos - pathPos,
                                      "<anonymized>");
                } else {
                    // Something went wrong. Anonymize the entire path to be
                    // safe.
                    name.Truncate(pathPos);
                    name += "<anonymized?!>";
                }
            }

            // We might have a location like this:
            //   inProcessTabChildGlobal?ownedBy=http://www.example.com/
            // The owner should be omitted if it's not a chrome: URI and we're
            // anonymizing.
            static const char* ownedByPrefix =
                "inProcessTabChildGlobal?ownedBy=";
            int ownedByPos = name.Find(ownedByPrefix);
            if (ownedByPos >= 0) {
                const char* chrome = "chrome:";
                int ownerPos = ownedByPos + strlen(ownedByPrefix);
                const nsDependentCSubstring& ownerFirstPart =
                    Substring(name, ownerPos, strlen(chrome));
                if (!ownerFirstPart.EqualsASCII(chrome)) {
                    name.Truncate(ownerPos);
                    name += "<anonymized>";
                }
            }
        }

        // A hack: replace forward slashes with '\\' so they aren't
        // treated as path separators.  Users of the reporters
        // (such as about:memory) have to undo this change.
        if (replaceSlashes)
            name.ReplaceChar('/', '\\');
    } else {
        name.AssignLiteral("null-principal");
    }
}

extern void
xpc::GetCurrentCompartmentName(JSContext* cx, nsCString& name)
{
    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    if (!global) {
        name.AssignLiteral("no global");
        return;
    }

    JSCompartment* compartment = GetObjectCompartment(global);
    int anonymizeID = 0;
    GetCompartmentName(compartment, name, &anonymizeID, false);
}

void
xpc::AddGCCallback(xpcGCCallback cb)
{
    XPCJSRuntime::Get()->AddGCCallback(cb);
}

void
xpc::RemoveGCCallback(xpcGCCallback cb)
{
    XPCJSRuntime::Get()->RemoveGCCallback(cb);
}

static int64_t
JSMainRuntimeGCHeapDistinguishedAmount()
{
    JSContext* cx = danger::GetJSContext();
    return int64_t(JS_GetGCParameter(cx, JSGC_TOTAL_CHUNKS)) *
           js::gc::ChunkSize;
}

static int64_t
JSMainRuntimeTemporaryPeakDistinguishedAmount()
{
    JSContext* cx = danger::GetJSContext();
    return JS::PeakSizeOfTemporary(cx);
}

static int64_t
JSMainRuntimeCompartmentsSystemDistinguishedAmount()
{
    JSContext* cx = danger::GetJSContext();
    return JS::SystemCompartmentCount(cx);
}

static int64_t
JSMainRuntimeCompartmentsUserDistinguishedAmount()
{
    JSContext* cx = XPCJSContext::Get()->Context();
    return JS::UserCompartmentCount(cx);
}

class JSMainRuntimeTemporaryPeakReporter final : public nsIMemoryReporter
{
    ~JSMainRuntimeTemporaryPeakReporter() {}

  public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize) override
    {
        MOZ_COLLECT_REPORT(
            "js-main-runtime-temporary-peak", KIND_OTHER, UNITS_BYTES,
            JSMainRuntimeTemporaryPeakDistinguishedAmount(),
            "Peak transient data size in the main JSRuntime (the current size "
            "of which is reported as "
            "'explicit/js-non-window/runtime/temporary').");

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(JSMainRuntimeTemporaryPeakReporter, nsIMemoryReporter)

// The REPORT* macros do an unconditional report.  The ZCREPORT* macros are for
// compartments and zones; they aggregate any entries smaller than
// SUNDRIES_THRESHOLD into the "sundries/gc-heap" and "sundries/malloc-heap"
// entries for the compartment.

#define SUNDRIES_THRESHOLD js::MemoryReportingSundriesThreshold()

#define REPORT(_path, _kind, _units, _amount, _desc) \
    handleReport->Callback(EmptyCString(), _path, \
                           nsIMemoryReporter::_kind, \
                           nsIMemoryReporter::_units, _amount, \
                           NS_LITERAL_CSTRING(_desc), data); \

#define REPORT_BYTES(_path, _kind, _amount, _desc) \
    REPORT(_path, _kind, UNITS_BYTES, _amount, _desc);

#define REPORT_GC_BYTES(_path, _amount, _desc) \
    do { \
        size_t amount = _amount;  /* evaluate _amount only once */ \
        handleReport->Callback(EmptyCString(), _path, \
                               nsIMemoryReporter::KIND_NONHEAP, \
                               nsIMemoryReporter::UNITS_BYTES, amount, \
                               NS_LITERAL_CSTRING(_desc), data); \
        gcTotal += amount; \
    } while (0)

// Report compartment/zone non-GC (KIND_HEAP) bytes.
#define ZCREPORT_BYTES(_path, _amount, _desc) \
    do { \
        /* Assign _descLiteral plus "" into a char* to prove that it's */ \
        /* actually a literal. */ \
        size_t amount = _amount;  /* evaluate _amount only once */ \
        if (amount >= SUNDRIES_THRESHOLD) { \
            handleReport->Callback(EmptyCString(), _path, \
                                   nsIMemoryReporter::KIND_HEAP, \
                                   nsIMemoryReporter::UNITS_BYTES, amount, \
                                   NS_LITERAL_CSTRING(_desc), data); \
        } else { \
            sundriesMallocHeap += amount; \
        } \
    } while (0)

// Report compartment/zone GC bytes.
#define ZCREPORT_GC_BYTES(_path, _amount, _desc) \
    do { \
        size_t amount = _amount;  /* evaluate _amount only once */ \
        if (amount >= SUNDRIES_THRESHOLD) { \
            handleReport->Callback(EmptyCString(), _path, \
                                   nsIMemoryReporter::KIND_NONHEAP, \
                                   nsIMemoryReporter::UNITS_BYTES, amount, \
                                   NS_LITERAL_CSTRING(_desc), data); \
            gcTotal += amount; \
        } else { \
            sundriesGCHeap += amount; \
        } \
    } while (0)

// Report runtime bytes.
#define RREPORT_BYTES(_path, _kind, _amount, _desc) \
    do { \
        size_t amount = _amount;  /* evaluate _amount only once */ \
        handleReport->Callback(EmptyCString(), _path, \
                               nsIMemoryReporter::_kind, \
                               nsIMemoryReporter::UNITS_BYTES, amount, \
                               NS_LITERAL_CSTRING(_desc), data); \
        rtTotal += amount; \
    } while (0)

// Report GC thing bytes.
#define MREPORT_BYTES(_path, _kind, _amount, _desc) \
    do { \
        size_t amount = _amount;  /* evaluate _amount only once */ \
        handleReport->Callback(EmptyCString(), _path, \
                               nsIMemoryReporter::_kind, \
                               nsIMemoryReporter::UNITS_BYTES, amount, \
                               NS_LITERAL_CSTRING(_desc), data); \
        gcThingTotal += amount; \
    } while (0)

MOZ_DEFINE_MALLOC_SIZE_OF(JSMallocSizeOf)

namespace xpc {

static void
ReportZoneStats(const JS::ZoneStats& zStats,
                const xpc::ZoneStatsExtras& extras,
                nsIHandleReportCallback* handleReport,
                nsISupports* data,
                bool anonymize,
                size_t* gcTotalOut = nullptr)
{
    const nsCString& pathPrefix = extras.pathPrefix;
    size_t gcTotal = 0, sundriesGCHeap = 0, sundriesMallocHeap = 0;

    MOZ_ASSERT(!gcTotalOut == zStats.isTotals);

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("symbols/gc-heap"),
        zStats.symbolsGCHeap,
        "Symbols.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("gc-heap-arena-admin"),
        zStats.gcHeapArenaAdmin,
        "Bookkeeping information and alignment padding within GC arenas.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("unused-gc-things"),
        zStats.unusedGCThings.totalSize(),
        "Unused GC thing cells within non-empty arenas.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("unique-id-map"),
        zStats.uniqueIdMap,
        "Address-independent cell identities.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("shape-tables"),
        zStats.shapeTables,
        "Tables storing shape information.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("lazy-scripts/gc-heap"),
        zStats.lazyScriptsGCHeap,
        "Scripts that haven't executed yet.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("lazy-scripts/malloc-heap"),
        zStats.lazyScriptsMallocHeap,
        "Lazy script tables containing closed-over bindings or inner functions.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("jit-codes-gc-heap"),
        zStats.jitCodesGCHeap,
        "References to executable code pools used by the JITs.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("object-groups/gc-heap"),
        zStats.objectGroupsGCHeap,
        "Classification and type inference information about objects.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("object-groups/malloc-heap"),
        zStats.objectGroupsMallocHeap,
        "Object group addenda.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("scopes/gc-heap"),
        zStats.scopesGCHeap,
        "Scope information for scripts.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("scopes/malloc-heap"),
        zStats.scopesMallocHeap,
        "Arrays of binding names and other binding-related data.");

    ZCREPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("regexp-shareds/gc-heap"),
        zStats.regExpSharedsGCHeap,
        "Shared compiled regexp data.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("regexp-shareds/malloc-heap"),
        zStats.regExpSharedsMallocHeap,
        "Shared compiled regexp data.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("type-pool"),
        zStats.typePool,
        "Type sets and related data.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("regexp-zone"),
        zStats.regexpZone,
        "The regexp zone and regexp data.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("jit-zone"),
        zStats.jitZone,
        "The JIT zone.");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("baseline/optimized-stubs"),
        zStats.baselineStubsOptimized,
        "The Baseline JIT's optimized IC stubs (excluding code).");

    ZCREPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("jit-cached-cfg"),
        zStats.cachedCFG,
        "The cached CFG to construct Ion code out of it.");

    size_t stringsNotableAboutMemoryGCHeap = 0;
    size_t stringsNotableAboutMemoryMallocHeap = 0;

    #define MAYBE_INLINE \
        "The characters may be inline or on the malloc heap."
    #define MAYBE_OVERALLOCATED \
        "Sometimes over-allocated to simplify string concatenation."

    for (size_t i = 0; i < zStats.notableStrings.length(); i++) {
        const JS::NotableStringInfo& info = zStats.notableStrings[i];

        MOZ_ASSERT(!zStats.isTotals);

        // We don't do notable string detection when anonymizing, because
        // there's a good chance its for crash submission, and the memory
        // required for notable string detection is high.
        MOZ_ASSERT(!anonymize);

        nsDependentCString notableString(info.buffer);

        // Viewing about:memory generates many notable strings which contain
        // "string(length=".  If we report these as notable, then we'll create
        // even more notable strings the next time we open about:memory (unless
        // there's a GC in the meantime), and so on ad infinitum.
        //
        // To avoid cluttering up about:memory like this, we stick notable
        // strings which contain "string(length=" into their own bucket.
#       define STRING_LENGTH "string(length="
        if (FindInReadable(NS_LITERAL_CSTRING(STRING_LENGTH), notableString)) {
            stringsNotableAboutMemoryGCHeap += info.gcHeapLatin1;
            stringsNotableAboutMemoryGCHeap += info.gcHeapTwoByte;
            stringsNotableAboutMemoryMallocHeap += info.mallocHeapLatin1;
            stringsNotableAboutMemoryMallocHeap += info.mallocHeapTwoByte;
            continue;
        }

        // Escape / to \ before we put notableString into the memory reporter
        // path, because we don't want any forward slashes in the string to
        // count as path separators.
        nsCString escapedString(notableString);
        escapedString.ReplaceSubstring("/", "\\");

        bool truncated = notableString.Length() < info.length;

        nsCString path = pathPrefix +
            nsPrintfCString("strings/" STRING_LENGTH "%zu, copies=%d, \"%s\"%s)/",
                            info.length, info.numCopies, escapedString.get(),
                            truncated ? " (truncated)" : "");

        if (info.gcHeapLatin1 > 0) {
            REPORT_GC_BYTES(path + NS_LITERAL_CSTRING("gc-heap/latin1"),
                info.gcHeapLatin1,
                "Latin1 strings. " MAYBE_INLINE);
        }

        if (info.gcHeapTwoByte > 0) {
            REPORT_GC_BYTES(path + NS_LITERAL_CSTRING("gc-heap/two-byte"),
                info.gcHeapTwoByte,
                "TwoByte strings. " MAYBE_INLINE);
        }

        if (info.mallocHeapLatin1 > 0) {
            REPORT_BYTES(path + NS_LITERAL_CSTRING("malloc-heap/latin1"),
                KIND_HEAP, info.mallocHeapLatin1,
                "Non-inline Latin1 string characters. " MAYBE_OVERALLOCATED);
        }

        if (info.mallocHeapTwoByte > 0) {
            REPORT_BYTES(path + NS_LITERAL_CSTRING("malloc-heap/two-byte"),
                KIND_HEAP, info.mallocHeapTwoByte,
                "Non-inline TwoByte string characters. " MAYBE_OVERALLOCATED);
        }
    }

    nsCString nonNotablePath = pathPrefix;
    nonNotablePath += (zStats.isTotals || anonymize)
                    ? NS_LITERAL_CSTRING("strings/")
                    : NS_LITERAL_CSTRING("strings/string(<non-notable strings>)/");

    if (zStats.stringInfo.gcHeapLatin1 > 0) {
        REPORT_GC_BYTES(nonNotablePath + NS_LITERAL_CSTRING("gc-heap/latin1"),
            zStats.stringInfo.gcHeapLatin1,
            "Latin1 strings. " MAYBE_INLINE);
    }

    if (zStats.stringInfo.gcHeapTwoByte > 0) {
        REPORT_GC_BYTES(nonNotablePath + NS_LITERAL_CSTRING("gc-heap/two-byte"),
            zStats.stringInfo.gcHeapTwoByte,
            "TwoByte strings. " MAYBE_INLINE);
    }

    if (zStats.stringInfo.mallocHeapLatin1 > 0) {
        REPORT_BYTES(nonNotablePath + NS_LITERAL_CSTRING("malloc-heap/latin1"),
            KIND_HEAP, zStats.stringInfo.mallocHeapLatin1,
            "Non-inline Latin1 string characters. " MAYBE_OVERALLOCATED);
    }

    if (zStats.stringInfo.mallocHeapTwoByte > 0) {
        REPORT_BYTES(nonNotablePath + NS_LITERAL_CSTRING("malloc-heap/two-byte"),
            KIND_HEAP, zStats.stringInfo.mallocHeapTwoByte,
            "Non-inline TwoByte string characters. " MAYBE_OVERALLOCATED);
    }

    if (stringsNotableAboutMemoryGCHeap > 0) {
        MOZ_ASSERT(!zStats.isTotals);
        REPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("strings/string(<about-memory>)/gc-heap"),
            stringsNotableAboutMemoryGCHeap,
            "Strings that contain the characters '" STRING_LENGTH "', which "
            "are probably from about:memory itself." MAYBE_INLINE
            " We filter them out rather than display them, because displaying "
            "them would create even more such strings every time about:memory "
            "is refreshed.");
    }

    if (stringsNotableAboutMemoryMallocHeap > 0) {
        MOZ_ASSERT(!zStats.isTotals);
        REPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("strings/string(<about-memory>)/malloc-heap"),
            KIND_HEAP, stringsNotableAboutMemoryMallocHeap,
            "Non-inline string characters of strings that contain the "
            "characters '" STRING_LENGTH "', which are probably from "
            "about:memory itself. " MAYBE_OVERALLOCATED
            " We filter them out rather than display them, because displaying "
            "them would create even more such strings every time about:memory "
            "is refreshed.");
    }

    const JS::ShapeInfo& shapeInfo = zStats.shapeInfo;
    if (shapeInfo.shapesGCHeapTree > 0) {
        REPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("shapes/gc-heap/tree"),
            shapeInfo.shapesGCHeapTree,
        "Shapes in a property tree.");
    }

    if (shapeInfo.shapesGCHeapDict > 0) {
        REPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("shapes/gc-heap/dict"),
            shapeInfo.shapesGCHeapDict,
        "Shapes in dictionary mode.");
    }

    if (shapeInfo.shapesGCHeapBase > 0) {
        REPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("shapes/gc-heap/base"),
            shapeInfo.shapesGCHeapBase,
            "Base shapes, which collate data common to many shapes.");
    }

    if (shapeInfo.shapesMallocHeapTreeTables > 0) {
        REPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("shapes/malloc-heap/tree-tables"),
            KIND_HEAP, shapeInfo.shapesMallocHeapTreeTables,
            "Property tables of shapes in a property tree.");
    }

    if (shapeInfo.shapesMallocHeapDictTables > 0) {
        REPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("shapes/malloc-heap/dict-tables"),
            KIND_HEAP, shapeInfo.shapesMallocHeapDictTables,
            "Property tables of shapes in dictionary mode.");
    }

    if (shapeInfo.shapesMallocHeapTreeKids > 0) {
        REPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("shapes/malloc-heap/tree-kids"),
            KIND_HEAP, shapeInfo.shapesMallocHeapTreeKids,
            "Kid hashes of shapes in a property tree.");
    }

    if (sundriesGCHeap > 0) {
        // We deliberately don't use ZCREPORT_GC_BYTES here.
        REPORT_GC_BYTES(pathPrefix + NS_LITERAL_CSTRING("sundries/gc-heap"),
            sundriesGCHeap,
            "The sum of all 'gc-heap' measurements that are too small to be "
            "worth showing individually.");
    }

    if (sundriesMallocHeap > 0) {
        // We deliberately don't use ZCREPORT_BYTES here.
        REPORT_BYTES(pathPrefix + NS_LITERAL_CSTRING("sundries/malloc-heap"),
            KIND_HEAP, sundriesMallocHeap,
            "The sum of all 'malloc-heap' measurements that are too small to "
            "be worth showing individually.");
    }

    if (gcTotalOut)
        *gcTotalOut += gcTotal;

#   undef STRING_LENGTH
}

static void
ReportClassStats(const ClassInfo& classInfo, const nsACString& path,
                 nsIHandleReportCallback* handleReport,
                 nsISupports* data, size_t& gcTotal)
{
    // We deliberately don't use ZCREPORT_BYTES, so that these per-class values
    // don't go into sundries.

    if (classInfo.objectsGCHeap > 0) {
        REPORT_GC_BYTES(path + NS_LITERAL_CSTRING("objects/gc-heap"),
            classInfo.objectsGCHeap,
            "Objects, including fixed slots.");
    }

    if (classInfo.objectsMallocHeapSlots > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/malloc-heap/slots"),
            KIND_HEAP, classInfo.objectsMallocHeapSlots,
            "Non-fixed object slots.");
    }

    if (classInfo.objectsMallocHeapElementsNormal > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/malloc-heap/elements/normal"),
            KIND_HEAP, classInfo.objectsMallocHeapElementsNormal,
            "Normal (non-wasm) indexed elements.");
    }

    if (classInfo.objectsMallocHeapElementsAsmJS > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/malloc-heap/elements/asm.js"),
            KIND_HEAP, classInfo.objectsMallocHeapElementsAsmJS,
            "asm.js array buffer elements allocated in the malloc heap.");
    }

    if (classInfo.objectsMallocHeapMisc > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/malloc-heap/misc"),
            KIND_HEAP, classInfo.objectsMallocHeapMisc,
            "Miscellaneous object data.");
    }

    if (classInfo.objectsNonHeapElementsNormal > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/non-heap/elements/normal"),
            KIND_NONHEAP, classInfo.objectsNonHeapElementsNormal,
            "Memory-mapped non-shared array buffer elements.");
    }

    if (classInfo.objectsNonHeapElementsShared > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/non-heap/elements/shared"),
            KIND_NONHEAP, classInfo.objectsNonHeapElementsShared,
            "Memory-mapped shared array buffer elements. These elements are "
            "shared between one or more runtimes; the reported size is divided "
            "by the buffer's refcount.");
    }

    // WebAssembly memories are always non-heap-allocated (mmap). We never put
    // these under sundries, because (a) in practice they're almost always
    // larger than the sundries threshold, and (b) we'd need a third category of
    // sundries ("non-heap"), which would be a pain.
    if (classInfo.objectsNonHeapElementsWasm > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/non-heap/elements/wasm"),
            KIND_NONHEAP, classInfo.objectsNonHeapElementsWasm,
            "wasm/asm.js array buffer elements allocated outside both the "
            "malloc heap and the GC heap.");
    }

    if (classInfo.objectsNonHeapCodeWasm > 0) {
        REPORT_BYTES(path + NS_LITERAL_CSTRING("objects/non-heap/code/wasm"),
            KIND_NONHEAP, classInfo.objectsNonHeapCodeWasm,
            "AOT-compiled wasm/asm.js code.");
    }

    // Although wasm guard pages aren't committed in memory they can be very
    // large and contribute greatly to vsize and so are worth reporting.
    if (classInfo.wasmGuardPages > 0) {
        REPORT_BYTES(NS_LITERAL_CSTRING("wasm-guard-pages"),
            KIND_OTHER, classInfo.wasmGuardPages,
            "Guard pages mapped after the end of wasm memories, reserved for "
            "optimization tricks, but not committed and thus never contributing"
            " to RSS, only vsize.");
    }
}

static void
ReportCompartmentStats(const JS::CompartmentStats& cStats,
                       const xpc::CompartmentStatsExtras& extras,
                       amIAddonManager* addonManager,
                       nsIHandleReportCallback* handleReport,
                       nsISupports* data, size_t* gcTotalOut = nullptr)
{
    static const nsDependentCString addonPrefix("explicit/add-ons/");

    size_t gcTotal = 0, sundriesGCHeap = 0, sundriesMallocHeap = 0;
    nsAutoCString cJSPathPrefix(extras.jsPathPrefix);
    nsAutoCString cDOMPathPrefix(extras.domPathPrefix);

    MOZ_ASSERT(!gcTotalOut == cStats.isTotals);

    // Only attempt to prefix if we got a location and the path wasn't already
    // prefixed.
    if (extras.location && addonManager &&
        cJSPathPrefix.Find(addonPrefix, false, 0, 0) != 0) {
        nsAutoCString addonId;
        bool ok;
        if (NS_SUCCEEDED(addonManager->MapURIToAddonID(extras.location,
                                                        addonId, &ok))
            && ok) {
            // Insert the add-on id as "add-ons/@id@/" after "explicit/" to
            // aggregate add-on compartments.
            static const size_t explicitLength = strlen("explicit/");
            addonId.Insert(NS_LITERAL_CSTRING("add-ons/"), 0);
            addonId += "/";
            cJSPathPrefix.Insert(addonId, explicitLength);
            cDOMPathPrefix.Insert(addonId, explicitLength);
        }
    }

    nsCString nonNotablePath = cJSPathPrefix;
    nonNotablePath += cStats.isTotals
                    ? NS_LITERAL_CSTRING("classes/")
                    : NS_LITERAL_CSTRING("classes/class(<non-notable classes>)/");

    ReportClassStats(cStats.classInfo, nonNotablePath, handleReport, data,
                     gcTotal);

    for (size_t i = 0; i < cStats.notableClasses.length(); i++) {
        MOZ_ASSERT(!cStats.isTotals);
        const JS::NotableClassInfo& classInfo = cStats.notableClasses[i];

        nsCString classPath = cJSPathPrefix +
            nsPrintfCString("classes/class(%s)/", classInfo.className_);

        ReportClassStats(classInfo, classPath, handleReport, data, gcTotal);
    }

    // Note that we use cDOMPathPrefix here.  This is because we measure orphan
    // DOM nodes in the JS reporter, but we want to report them in a "dom"
    // sub-tree rather than a "js" sub-tree.
    ZCREPORT_BYTES(cDOMPathPrefix + NS_LITERAL_CSTRING("orphan-nodes"),
        cStats.objectsPrivate,
        "Orphan DOM nodes, i.e. those that are only reachable from JavaScript "
        "objects.");

    ZCREPORT_GC_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("scripts/gc-heap"),
        cStats.scriptsGCHeap,
        "JSScript instances. There is one per user-defined function in a "
        "script, and one for the top-level code in a script.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("scripts/malloc-heap/data"),
        cStats.scriptsMallocHeapData,
        "Various variable-length tables in JSScripts.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("baseline/data"),
        cStats.baselineData,
        "The Baseline JIT's compilation data (BaselineScripts).");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("baseline/fallback-stubs"),
        cStats.baselineStubsFallback,
        "The Baseline JIT's fallback IC stubs (excluding code).");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("ion-data"),
        cStats.ionData,
        "The IonMonkey JIT's compilation data (IonScripts).");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("type-inference/type-scripts"),
        cStats.typeInferenceTypeScripts,
        "Type sets associated with scripts.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("type-inference/allocation-site-tables"),
        cStats.typeInferenceAllocationSiteTables,
        "Tables of type objects associated with allocation sites.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("type-inference/array-type-tables"),
        cStats.typeInferenceArrayTypeTables,
        "Tables of type objects associated with array literals.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("type-inference/object-type-tables"),
        cStats.typeInferenceObjectTypeTables,
        "Tables of type objects associated with object literals.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("compartment-object"),
        cStats.compartmentObject,
        "The JSCompartment object itself.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("compartment-tables"),
        cStats.compartmentTables,
        "Compartment-wide tables storing object group information and wasm instances.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("inner-views"),
        cStats.innerViewsTable,
        "The table for array buffer inner views.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("lazy-array-buffers"),
        cStats.lazyArrayBuffersTable,
        "The table for typed object lazy array buffers.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("object-metadata"),
        cStats.objectMetadataTable,
        "The table used by debugging tools for tracking object metadata");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("cross-compartment-wrapper-table"),
        cStats.crossCompartmentWrappersTable,
        "The cross-compartment wrapper table.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("saved-stacks-set"),
        cStats.savedStacksSet,
        "The saved stacks set.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("non-syntactic-lexical-scopes-table"),
        cStats.nonSyntacticLexicalScopesTable,
        "The non-syntactic lexical scopes table.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("template-literal-map"),
        cStats.templateLiteralMap,
        "The template literal registry.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("jit-compartment"),
        cStats.jitCompartment,
        "The JIT compartment.");

    ZCREPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("private-data"),
        cStats.privateData,
        "Extra data attached to the compartment by XPConnect, including "
        "its wrapped-js.");

    if (sundriesGCHeap > 0) {
        // We deliberately don't use ZCREPORT_GC_BYTES here.
        REPORT_GC_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("sundries/gc-heap"),
            sundriesGCHeap,
            "The sum of all 'gc-heap' measurements that are too small to be "
            "worth showing individually.");
    }

    if (sundriesMallocHeap > 0) {
        // We deliberately don't use ZCREPORT_BYTES here.
        REPORT_BYTES(cJSPathPrefix + NS_LITERAL_CSTRING("sundries/malloc-heap"),
            KIND_HEAP, sundriesMallocHeap,
            "The sum of all 'malloc-heap' measurements that are too small to "
            "be worth showing individually.");
    }

    if (gcTotalOut)
        *gcTotalOut += gcTotal;
}

static void
ReportScriptSourceStats(const ScriptSourceInfo& scriptSourceInfo,
                        const nsACString& path,
                        nsIHandleReportCallback* handleReport,
                        nsISupports* data, size_t& rtTotal)
{
    if (scriptSourceInfo.misc > 0) {
        RREPORT_BYTES(path + NS_LITERAL_CSTRING("misc"),
            KIND_HEAP, scriptSourceInfo.misc,
            "Miscellaneous data relating to JavaScript source code.");
    }
}

static void
ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats& rtStats,
                                 const nsACString& rtPath,
                                 amIAddonManager* addonManager,
                                 nsIHandleReportCallback* handleReport,
                                 nsISupports* data,
                                 bool anonymize,
                                 size_t* rtTotalOut)
{
    size_t gcTotal = 0;

    for (size_t i = 0; i < rtStats.zoneStatsVector.length(); i++) {
        const JS::ZoneStats& zStats = rtStats.zoneStatsVector[i];
        const xpc::ZoneStatsExtras* extras =
          static_cast<const xpc::ZoneStatsExtras*>(zStats.extra);
        ReportZoneStats(zStats, *extras, handleReport, data, anonymize,
                        &gcTotal);
    }

    for (size_t i = 0; i < rtStats.compartmentStatsVector.length(); i++) {
        const JS::CompartmentStats& cStats = rtStats.compartmentStatsVector[i];
        const xpc::CompartmentStatsExtras* extras =
            static_cast<const xpc::CompartmentStatsExtras*>(cStats.extra);

        ReportCompartmentStats(cStats, *extras, addonManager, handleReport,
                               data, &gcTotal);
    }

    // Report the rtStats.runtime numbers under "runtime/", and compute their
    // total for later.

    size_t rtTotal = 0;

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/runtime-object"),
        KIND_HEAP, rtStats.runtime.object,
        "The JSRuntime object.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/atoms-table"),
        KIND_HEAP, rtStats.runtime.atomsTable,
        "The atoms table.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/atoms-mark-bitmaps"),
        KIND_HEAP, rtStats.runtime.atomsMarkBitmaps,
        "Mark bitmaps for atoms held by each zone.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/contexts"),
        KIND_HEAP, rtStats.runtime.contexts,
        "JSContext objects and structures that belong to them.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/temporary"),
        KIND_HEAP, rtStats.runtime.temporary,
        "Transient data (mostly parse nodes) held by the JSRuntime during "
        "compilation.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/interpreter-stack"),
        KIND_HEAP, rtStats.runtime.interpreterStack,
        "JS interpreter frames.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/math-cache"),
        KIND_HEAP, rtStats.runtime.mathCache,
        "The math cache.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/shared-immutable-strings-cache"),
        KIND_HEAP, rtStats.runtime.sharedImmutableStringsCache,
        "Immutable strings (such as JS scripts' source text) shared across all JSRuntimes.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/shared-intl-data"),
        KIND_HEAP, rtStats.runtime.sharedIntlData,
        "Shared internationalization data.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/uncompressed-source-cache"),
        KIND_HEAP, rtStats.runtime.uncompressedSourceCache,
        "The uncompressed source code cache.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/script-data"),
        KIND_HEAP, rtStats.runtime.scriptData,
        "The table holding script data shared in the runtime.");

    nsCString nonNotablePath =
        rtPath + nsPrintfCString("runtime/script-sources/source(scripts=%d, <non-notable files>)/",
                                 rtStats.runtime.scriptSourceInfo.numScripts);

    ReportScriptSourceStats(rtStats.runtime.scriptSourceInfo,
                            nonNotablePath, handleReport, data, rtTotal);

    for (size_t i = 0; i < rtStats.runtime.notableScriptSources.length(); i++) {
        const JS::NotableScriptSourceInfo& scriptSourceInfo =
            rtStats.runtime.notableScriptSources[i];

        // Escape / to \ before we put the filename into the memory reporter
        // path, because we don't want any forward slashes in the string to
        // count as path separators. Consumers of memory reporters (e.g.
        // about:memory) will convert them back to / after doing path
        // splitting.
        nsCString escapedFilename;
        if (anonymize) {
            escapedFilename.AppendPrintf("<anonymized-source-%d>", int(i));
        } else {
            nsDependentCString filename(scriptSourceInfo.filename_);
            escapedFilename.Append(filename);
            escapedFilename.ReplaceSubstring("/", "\\");
        }

        nsCString notablePath = rtPath +
            nsPrintfCString("runtime/script-sources/source(scripts=%d, %s)/",
                            scriptSourceInfo.numScripts, escapedFilename.get());

        ReportScriptSourceStats(scriptSourceInfo, notablePath,
                                handleReport, data, rtTotal);
    }

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/code/ion"),
        KIND_NONHEAP, rtStats.runtime.code.ion,
        "Code generated by the IonMonkey JIT.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/code/baseline"),
        KIND_NONHEAP, rtStats.runtime.code.baseline,
        "Code generated by the Baseline JIT.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/code/regexp"),
        KIND_NONHEAP, rtStats.runtime.code.regexp,
        "Code generated by the regexp JIT.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/code/other"),
        KIND_NONHEAP, rtStats.runtime.code.other,
        "Code generated by the JITs for wrappers and trampolines.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/code/unused"),
        KIND_NONHEAP, rtStats.runtime.code.unused,
        "Memory allocated by one of the JITs to hold code, but which is "
        "currently unused.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/marker"),
        KIND_HEAP, rtStats.runtime.gc.marker,
        "The GC mark stack and gray roots.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/nursery-committed"),
        KIND_NONHEAP, rtStats.runtime.gc.nurseryCommitted,
        "Memory being used by the GC's nursery.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/nursery-malloced-buffers"),
        KIND_HEAP, rtStats.runtime.gc.nurseryMallocedBuffers,
        "Out-of-line slots and elements belonging to objects in the nursery.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/store-buffer/vals"),
        KIND_HEAP, rtStats.runtime.gc.storeBufferVals,
        "Values in the store buffer.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/store-buffer/cells"),
        KIND_HEAP, rtStats.runtime.gc.storeBufferCells,
        "Cells in the store buffer.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/store-buffer/slots"),
        KIND_HEAP, rtStats.runtime.gc.storeBufferSlots,
        "Slots in the store buffer.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/store-buffer/whole-cells"),
        KIND_HEAP, rtStats.runtime.gc.storeBufferWholeCells,
        "Whole cells in the store buffer.");

    RREPORT_BYTES(rtPath + NS_LITERAL_CSTRING("runtime/gc/store-buffer/generics"),
        KIND_HEAP, rtStats.runtime.gc.storeBufferGenerics,
        "Generic things in the store buffer.");

    if (rtTotalOut)
        *rtTotalOut = rtTotal;

    // Report GC numbers that don't belong to a compartment.

    // We don't want to report decommitted memory in "explicit", so we just
    // change the leading "explicit/" to "decommitted/".
    nsCString rtPath2(rtPath);
    rtPath2.Replace(0, strlen("explicit"), NS_LITERAL_CSTRING("decommitted"));
    REPORT_GC_BYTES(rtPath2 + NS_LITERAL_CSTRING("gc-heap/decommitted-arenas"),
        rtStats.gcHeapDecommittedArenas,
        "GC arenas in non-empty chunks that is decommitted, i.e. it takes up "
        "address space but no physical memory or swap space.");

    REPORT_GC_BYTES(rtPath + NS_LITERAL_CSTRING("gc-heap/unused-chunks"),
        rtStats.gcHeapUnusedChunks,
        "Empty GC chunks which will soon be released unless claimed for new "
        "allocations.");

    REPORT_GC_BYTES(rtPath + NS_LITERAL_CSTRING("gc-heap/unused-arenas"),
        rtStats.gcHeapUnusedArenas,
        "Empty GC arenas within non-empty chunks.");

    REPORT_GC_BYTES(rtPath + NS_LITERAL_CSTRING("gc-heap/chunk-admin"),
        rtStats.gcHeapChunkAdmin,
        "Bookkeeping information within GC chunks.");

    // gcTotal is the sum of everything we've reported for the GC heap.  It
    // should equal rtStats.gcHeapChunkTotal.
    MOZ_ASSERT(gcTotal == rtStats.gcHeapChunkTotal);
}

void
ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats& rtStats,
                                 const nsACString& rtPath,
                                 nsIHandleReportCallback* handleReport,
                                 nsISupports* data,
                                 bool anonymize,
                                 size_t* rtTotalOut)
{
    nsCOMPtr<amIAddonManager> am;
    if (XRE_IsParentProcess()) {
        // Only try to access the service from the main process.
        am = do_GetService("@mozilla.org/addons/integration;1");
    }
    ReportJSRuntimeExplicitTreeStats(rtStats, rtPath, am.get(), handleReport,
                                     data, anonymize, rtTotalOut);
}


} // namespace xpc

class JSMainRuntimeCompartmentsReporter final : public nsIMemoryReporter
{

    ~JSMainRuntimeCompartmentsReporter() {}

  public:
    NS_DECL_ISUPPORTS

    struct Data {
        int anonymizeID;
        js::Vector<nsCString, 0, js::SystemAllocPolicy> paths;
    };

    static void CompartmentCallback(JSContext* cx, void* vdata, JSCompartment* c) {
        // silently ignore OOM errors
        Data* data = static_cast<Data*>(vdata);
        nsCString path;
        GetCompartmentName(c, path, &data->anonymizeID, /* replaceSlashes = */ true);
        path.Insert(js::IsSystemCompartment(c)
                    ? NS_LITERAL_CSTRING("js-main-runtime-compartments/system/")
                    : NS_LITERAL_CSTRING("js-main-runtime-compartments/user/"),
                    0);
        mozilla::Unused << data->paths.append(path);
    }

    NS_IMETHOD CollectReports(nsIHandleReportCallback* handleReport,
                              nsISupports* data, bool anonymize) override
    {
        // First we collect the compartment paths.  Then we report them.  Doing
        // the two steps interleaved is a bad idea, because calling
        // |handleReport| from within CompartmentCallback() leads to all manner
        // of assertions.

        Data d;
        d.anonymizeID = anonymize ? 1 : 0;
        JS_IterateCompartments(XPCJSContext::Get()->Context(),
                               &d, CompartmentCallback);

        for (size_t i = 0; i < d.paths.length(); i++)
            REPORT(nsCString(d.paths[i]), KIND_OTHER, UNITS_COUNT, 1,
                "A live compartment in the main JSRuntime.");

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(JSMainRuntimeCompartmentsReporter, nsIMemoryReporter)

MOZ_DEFINE_MALLOC_SIZE_OF(OrphanMallocSizeOf)

namespace xpc {

static size_t
SizeOfTreeIncludingThis(nsINode* tree, SizeOfState& aState)
{
    size_t n = tree->SizeOfIncludingThis(aState);
    for (nsIContent* child = tree->GetFirstChild(); child; child = child->GetNextNode(tree))
        n += child->SizeOfIncludingThis(aState);

    return n;
}

class OrphanReporter : public JS::ObjectPrivateVisitor
{
  public:
    explicit OrphanReporter(GetISupportsFun aGetISupports)
      : JS::ObjectPrivateVisitor(aGetISupports)
      , mState(OrphanMallocSizeOf)
    {}

    virtual size_t sizeOfIncludingThis(nsISupports* aSupports) override
    {
        size_t n = 0;
        nsCOMPtr<nsINode> node = do_QueryInterface(aSupports);
        // https://bugzilla.mozilla.org/show_bug.cgi?id=773533#c11 explains
        // that we have to skip XBL elements because they violate certain
        // assumptions.  Yuk.
        if (node && !node->IsInUncomposedDoc() &&
            !(node->IsElement() && node->AsElement()->IsInNamespace(kNameSpaceID_XBL)))
        {
            // This is an orphan node.  If we haven't already handled the
            // sub-tree that this node belongs to, measure the sub-tree's size
            // and then record its root so we don't measure it again.
            nsCOMPtr<nsINode> orphanTree = node->SubtreeRoot();
            if (orphanTree && !mState.HaveSeenPtr(orphanTree.get())) {
                n += SizeOfTreeIncludingThis(orphanTree, mState);
            }
        }
        return n;
    }

  private:
    SizeOfState mState;
};

#ifdef DEBUG
static bool
StartsWithExplicit(nsACString& s)
{
    return StringBeginsWith(s, NS_LITERAL_CSTRING("explicit/"));
}
#endif

class XPCJSRuntimeStats : public JS::RuntimeStats
{
    WindowPaths* mWindowPaths;
    WindowPaths* mTopWindowPaths;
    bool mGetLocations;
    int mAnonymizeID;

  public:
    XPCJSRuntimeStats(WindowPaths* windowPaths, WindowPaths* topWindowPaths,
                      bool getLocations, bool anonymize)
      : JS::RuntimeStats(JSMallocSizeOf),
        mWindowPaths(windowPaths),
        mTopWindowPaths(topWindowPaths),
        mGetLocations(getLocations),
        mAnonymizeID(anonymize ? 1 : 0)
    {}

    ~XPCJSRuntimeStats() {
        for (size_t i = 0; i != compartmentStatsVector.length(); ++i)
            delete static_cast<xpc::CompartmentStatsExtras*>(compartmentStatsVector[i].extra);


        for (size_t i = 0; i != zoneStatsVector.length(); ++i)
            delete static_cast<xpc::ZoneStatsExtras*>(zoneStatsVector[i].extra);
    }

    virtual void initExtraZoneStats(JS::Zone* zone, JS::ZoneStats* zStats) override {
        // Get the compartment's global.
        AutoSafeJSContext cx;
        JSCompartment* comp = js::GetAnyCompartmentInZone(zone);
        xpc::ZoneStatsExtras* extras = new xpc::ZoneStatsExtras;
        extras->pathPrefix.AssignLiteral("explicit/js-non-window/zones/");
        RootedObject global(cx, JS_GetGlobalForCompartmentOrNull(cx, comp));
        if (global) {
            RefPtr<nsGlobalWindow> window;
            if (NS_SUCCEEDED(UNWRAP_OBJECT(Window, global, window))) {
                // The global is a |window| object.  Use the path prefix that
                // we should have already created for it.
                if (mTopWindowPaths->Get(window->WindowID(),
                                         &extras->pathPrefix))
                    extras->pathPrefix.AppendLiteral("/js-");
            }
        }

        extras->pathPrefix += nsPrintfCString("zone(0x%p)/", (void*)zone);

        MOZ_ASSERT(StartsWithExplicit(extras->pathPrefix));

        zStats->extra = extras;
    }

    virtual void initExtraCompartmentStats(JSCompartment* c,
                                           JS::CompartmentStats* cstats) override
    {
        xpc::CompartmentStatsExtras* extras = new xpc::CompartmentStatsExtras;
        nsCString cName;
        GetCompartmentName(c, cName, &mAnonymizeID, /* replaceSlashes = */ true);
        CompartmentPrivate* cp = CompartmentPrivate::Get(c);
        if (cp) {
            if (mGetLocations) {
                cp->GetLocationURI(CompartmentPrivate::LocationHintAddon,
                                   getter_AddRefs(extras->location));
            }
            // Note: cannot use amIAddonManager implementation at this point,
            // as it is a JS service and the JS heap is currently not idle.
            // Otherwise, we could have computed the add-on id at this point.
        }

        // Get the compartment's global.
        AutoSafeJSContext cx;
        bool needZone = true;
        RootedObject global(cx, JS_GetGlobalForCompartmentOrNull(cx, c));
        if (global) {
            RefPtr<nsGlobalWindow> window;
            if (NS_SUCCEEDED(UNWRAP_OBJECT(Window, global, window))) {
                // The global is a |window| object.  Use the path prefix that
                // we should have already created for it.
                if (mWindowPaths->Get(window->WindowID(),
                                      &extras->jsPathPrefix)) {
                    extras->domPathPrefix.Assign(extras->jsPathPrefix);
                    extras->domPathPrefix.AppendLiteral("/dom/");
                    extras->jsPathPrefix.AppendLiteral("/js-");
                    needZone = false;
                } else {
                    extras->jsPathPrefix.AssignLiteral("explicit/js-non-window/zones/");
                    extras->domPathPrefix.AssignLiteral("explicit/dom/unknown-window-global?!/");
                }
            } else {
                extras->jsPathPrefix.AssignLiteral("explicit/js-non-window/zones/");
                extras->domPathPrefix.AssignLiteral("explicit/dom/non-window-global?!/");
            }
        } else {
            extras->jsPathPrefix.AssignLiteral("explicit/js-non-window/zones/");
            extras->domPathPrefix.AssignLiteral("explicit/dom/no-global?!/");
        }

        if (needZone)
            extras->jsPathPrefix += nsPrintfCString("zone(0x%p)/", (void*)js::GetCompartmentZone(c));

        extras->jsPathPrefix += NS_LITERAL_CSTRING("compartment(") + cName + NS_LITERAL_CSTRING(")/");

        // extras->jsPathPrefix is used for almost all the compartment-specific
        // reports. At this point it has the form
        // "<something>compartment(<cname>)/".
        //
        // extras->domPathPrefix is used for DOM orphan nodes, which are
        // counted by the JS reporter but reported as part of the DOM
        // measurements. At this point it has the form "<something>/dom/" if
        // this compartment belongs to an nsGlobalWindow, and
        // "explicit/dom/<something>?!/" otherwise (in which case it shouldn't
        // be used, because non-nsGlobalWindow compartments shouldn't have
        // orphan DOM nodes).

        MOZ_ASSERT(StartsWithExplicit(extras->jsPathPrefix));
        MOZ_ASSERT(StartsWithExplicit(extras->domPathPrefix));

        cstats->extra = extras;
    }
};

void
JSReporter::CollectReports(WindowPaths* windowPaths,
                           WindowPaths* topWindowPaths,
                           nsIHandleReportCallback* handleReport,
                           nsISupports* data,
                           bool anonymize)
{
    XPCJSRuntime* xpcrt = nsXPConnect::GetRuntimeInstance();

    // In the first step we get all the stats and stash them in a local
    // data structure.  In the second step we pass all the stashed stats to
    // the callback.  Separating these steps is important because the
    // callback may be a JS function, and executing JS while getting these
    // stats seems like a bad idea.

    nsCOMPtr<amIAddonManager> addonManager;
    if (XRE_IsParentProcess()) {
        // Only try to access the service from the main process.
        addonManager = do_GetService("@mozilla.org/addons/integration;1");
    }
    bool getLocations = !!addonManager;
    XPCJSRuntimeStats rtStats(windowPaths, topWindowPaths, getLocations,
                              anonymize);
    OrphanReporter orphanReporter(XPCConvert::GetISupportsFromJSObject);
    JSContext* cx = XPCJSContext::Get()->Context();
    if (!JS::CollectRuntimeStats(cx, &rtStats, &orphanReporter,
                                 anonymize))
    {
        return;
    }

    JS::CollectTraceLoggerStateStats(&rtStats);

    size_t xpcJSRuntimeSize = xpcrt->SizeOfIncludingThis(JSMallocSizeOf);

    size_t wrappedJSSize = xpcrt->GetMultiCompartmentWrappedJSMap()->SizeOfWrappedJS(JSMallocSizeOf);

    XPCWrappedNativeScope::ScopeSizeInfo sizeInfo(JSMallocSizeOf);
    XPCWrappedNativeScope::AddSizeOfAllScopesIncludingThis(&sizeInfo);

    mozJSComponentLoader* loader = mozJSComponentLoader::Get();
    size_t jsComponentLoaderSize = loader ? loader->SizeOfIncludingThis(JSMallocSizeOf) : 0;

    // This is the second step (see above).  First we report stuff in the
    // "explicit" tree, then we report other stuff.

    size_t rtTotal = 0;
    xpc::ReportJSRuntimeExplicitTreeStats(rtStats,
                                          NS_LITERAL_CSTRING("explicit/js-non-window/"),
                                          addonManager, handleReport, data,
                                          anonymize, &rtTotal);

    // Report the sums of the compartment numbers.
    xpc::CompartmentStatsExtras cExtrasTotal;
    cExtrasTotal.jsPathPrefix.AssignLiteral("js-main-runtime/compartments/");
    cExtrasTotal.domPathPrefix.AssignLiteral("window-objects/dom/");
    ReportCompartmentStats(rtStats.cTotals, cExtrasTotal, addonManager,
                           handleReport, data);

    xpc::ZoneStatsExtras zExtrasTotal;
    zExtrasTotal.pathPrefix.AssignLiteral("js-main-runtime/zones/");
    ReportZoneStats(rtStats.zTotals, zExtrasTotal, handleReport, data,
                    anonymize);

    // Report the sum of the runtime/ numbers.
    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime/runtime"),
        KIND_OTHER, rtTotal,
        "The sum of all measurements under 'explicit/js-non-window/runtime/'.");

    // Report the numbers for memory used by tracelogger.
    REPORT_BYTES(NS_LITERAL_CSTRING("tracelogger"),
        KIND_OTHER, rtStats.runtime.tracelogger,
        "The memory used for the tracelogger, including the graph and events.");

    // Report the numbers for memory outside of compartments.

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime/gc-heap/unused-chunks"),
        KIND_OTHER, rtStats.gcHeapUnusedChunks,
        "The same as 'explicit/js-non-window/gc-heap/unused-chunks'.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime/gc-heap/unused-arenas"),
        KIND_OTHER, rtStats.gcHeapUnusedArenas,
        "The same as 'explicit/js-non-window/gc-heap/unused-arenas'.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime/gc-heap/chunk-admin"),
        KIND_OTHER, rtStats.gcHeapChunkAdmin,
        "The same as 'explicit/js-non-window/gc-heap/chunk-admin'.");

    // Report a breakdown of the committed GC space.

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/chunks"),
        KIND_OTHER, rtStats.gcHeapUnusedChunks,
        "The same as 'explicit/js-non-window/gc-heap/unused-chunks'.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/arenas"),
        KIND_OTHER, rtStats.gcHeapUnusedArenas,
        "The same as 'explicit/js-non-window/gc-heap/unused-arenas'.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/objects"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.object,
        "Unused object cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/strings"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.string,
        "Unused string cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/symbols"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.symbol,
        "Unused symbol cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/shapes"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.shape,
        "Unused shape cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/base-shapes"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.baseShape,
        "Unused base shape cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/object-groups"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.objectGroup,
        "Unused object group cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/scopes"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.scope,
        "Unused scope cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/scripts"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.script,
        "Unused script cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/lazy-scripts"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.lazyScript,
        "Unused lazy script cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/jitcode"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.jitcode,
        "Unused jitcode cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/unused/gc-things/regexp-shareds"),
        KIND_OTHER, rtStats.zTotals.unusedGCThings.regExpShared,
        "Unused regexpshared cells within non-empty arenas.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/chunk-admin"),
        KIND_OTHER, rtStats.gcHeapChunkAdmin,
        "The same as 'explicit/js-non-window/gc-heap/chunk-admin'.");

    REPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/arena-admin"),
        KIND_OTHER, rtStats.zTotals.gcHeapArenaAdmin,
        "The same as 'js-main-runtime/zones/gc-heap-arena-admin'.");

    size_t gcThingTotal = 0;

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/objects"),
        KIND_OTHER, rtStats.cTotals.classInfo.objectsGCHeap,
        "Used object cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/strings"),
        KIND_OTHER, rtStats.zTotals.stringInfo.sizeOfLiveGCThings(),
        "Used string cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/symbols"),
        KIND_OTHER, rtStats.zTotals.symbolsGCHeap,
        "Used symbol cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/shapes"),
        KIND_OTHER,
        rtStats.zTotals.shapeInfo.shapesGCHeapTree + rtStats.zTotals.shapeInfo.shapesGCHeapDict,
        "Used shape cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/base-shapes"),
        KIND_OTHER, rtStats.zTotals.shapeInfo.shapesGCHeapBase,
        "Used base shape cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/object-groups"),
        KIND_OTHER, rtStats.zTotals.objectGroupsGCHeap,
        "Used object group cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/scopes"),
        KIND_OTHER, rtStats.zTotals.scopesGCHeap,
        "Used scope cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/scripts"),
        KIND_OTHER, rtStats.cTotals.scriptsGCHeap,
        "Used script cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/lazy-scripts"),
        KIND_OTHER, rtStats.zTotals.lazyScriptsGCHeap,
        "Used lazy script cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/jitcode"),
        KIND_OTHER, rtStats.zTotals.jitCodesGCHeap,
        "Used jitcode cells.");

    MREPORT_BYTES(NS_LITERAL_CSTRING("js-main-runtime-gc-heap-committed/used/gc-things/regexp-shareds"),
        KIND_OTHER, rtStats.zTotals.regExpSharedsGCHeap,
        "Used regexpshared cells.");

    MOZ_ASSERT(gcThingTotal == rtStats.gcHeapGCThings);

    // Report xpconnect.

    REPORT_BYTES(NS_LITERAL_CSTRING("explicit/xpconnect/runtime"),
        KIND_HEAP, xpcJSRuntimeSize,
        "The XPConnect runtime.");

    REPORT_BYTES(NS_LITERAL_CSTRING("explicit/xpconnect/wrappedjs"),
        KIND_HEAP, wrappedJSSize,
        "Wrappers used to implement XPIDL interfaces with JS.");

    REPORT_BYTES(NS_LITERAL_CSTRING("explicit/xpconnect/scopes"),
        KIND_HEAP, sizeInfo.mScopeAndMapSize,
        "XPConnect scopes.");

    REPORT_BYTES(NS_LITERAL_CSTRING("explicit/xpconnect/proto-iface-cache"),
        KIND_HEAP, sizeInfo.mProtoAndIfaceCacheSize,
        "Prototype and interface binding caches.");

    REPORT_BYTES(NS_LITERAL_CSTRING("explicit/xpconnect/js-component-loader"),
        KIND_HEAP, jsComponentLoaderSize,
        "XPConnect's JS component loader.");
}

static nsresult
JSSizeOfTab(JSObject* objArg, size_t* jsObjectsSize, size_t* jsStringsSize,
            size_t* jsPrivateSize, size_t* jsOtherSize)
{
    JSContext* cx = XPCJSContext::Get()->Context();
    JS::RootedObject obj(cx, objArg);

    TabSizes sizes;
    OrphanReporter orphanReporter(XPCConvert::GetISupportsFromJSObject);
    NS_ENSURE_TRUE(JS::AddSizeOfTab(cx, obj, moz_malloc_size_of,
                                    &orphanReporter, &sizes),
                   NS_ERROR_OUT_OF_MEMORY);

    *jsObjectsSize = sizes.objects;
    *jsStringsSize = sizes.strings;
    *jsPrivateSize = sizes.private_;
    *jsOtherSize   = sizes.other;
    return NS_OK;
}

} // namespace xpc

static void
AccumulateTelemetryCallback(int id, uint32_t sample, const char* key)
{
    switch (id) {
      case JS_TELEMETRY_GC_REASON:
        Telemetry::Accumulate(Telemetry::GC_REASON_2, sample);
        break;
      case JS_TELEMETRY_GC_IS_ZONE_GC:
        Telemetry::Accumulate(Telemetry::GC_IS_COMPARTMENTAL, sample);
        break;
      case JS_TELEMETRY_GC_MS:
        Telemetry::Accumulate(Telemetry::GC_MS, sample);
        break;
      case JS_TELEMETRY_GC_BUDGET_MS:
        Telemetry::Accumulate(Telemetry::GC_BUDGET_MS, sample);
        break;
      case JS_TELEMETRY_GC_BUDGET_OVERRUN:
        Telemetry::Accumulate(Telemetry::GC_BUDGET_OVERRUN, sample);
        break;
      case JS_TELEMETRY_GC_ANIMATION_MS:
        Telemetry::Accumulate(Telemetry::GC_ANIMATION_MS, sample);
        break;
      case JS_TELEMETRY_GC_MAX_PAUSE_MS:
        Telemetry::Accumulate(Telemetry::GC_MAX_PAUSE_MS, sample);
        break;
      case JS_TELEMETRY_GC_MAX_PAUSE_MS_2:
        Telemetry::Accumulate(Telemetry::GC_MAX_PAUSE_MS_2, sample);
        break;
      case JS_TELEMETRY_GC_MARK_MS:
        Telemetry::Accumulate(Telemetry::GC_MARK_MS, sample);
        break;
      case JS_TELEMETRY_GC_SWEEP_MS:
        Telemetry::Accumulate(Telemetry::GC_SWEEP_MS, sample);
        break;
      case JS_TELEMETRY_GC_COMPACT_MS:
        Telemetry::Accumulate(Telemetry::GC_COMPACT_MS, sample);
        break;
      case JS_TELEMETRY_GC_MARK_ROOTS_MS:
        Telemetry::Accumulate(Telemetry::GC_MARK_ROOTS_MS, sample);
        break;
      case JS_TELEMETRY_GC_MARK_GRAY_MS:
        Telemetry::Accumulate(Telemetry::GC_MARK_GRAY_MS, sample);
        break;
      case JS_TELEMETRY_GC_SLICE_MS:
        Telemetry::Accumulate(Telemetry::GC_SLICE_MS, sample);
        break;
      case JS_TELEMETRY_GC_SLOW_PHASE:
        Telemetry::Accumulate(Telemetry::GC_SLOW_PHASE, sample);
        break;
      case JS_TELEMETRY_GC_SLOW_TASK:
        Telemetry::Accumulate(Telemetry::GC_SLOW_TASK, sample);
        break;
      case JS_TELEMETRY_GC_MMU_50:
        Telemetry::Accumulate(Telemetry::GC_MMU_50, sample);
        break;
      case JS_TELEMETRY_GC_RESET:
        Telemetry::Accumulate(Telemetry::GC_RESET, sample);
        break;
      case JS_TELEMETRY_GC_RESET_REASON:
        Telemetry::Accumulate(Telemetry::GC_RESET_REASON, sample);
        break;
      case JS_TELEMETRY_GC_INCREMENTAL_DISABLED:
        Telemetry::Accumulate(Telemetry::GC_INCREMENTAL_DISABLED, sample);
        break;
      case JS_TELEMETRY_GC_NON_INCREMENTAL:
        Telemetry::Accumulate(Telemetry::GC_NON_INCREMENTAL, sample);
        break;
      case JS_TELEMETRY_GC_NON_INCREMENTAL_REASON:
        Telemetry::Accumulate(Telemetry::GC_NON_INCREMENTAL_REASON, sample);
        break;
      case JS_TELEMETRY_GC_SCC_SWEEP_TOTAL_MS:
        Telemetry::Accumulate(Telemetry::GC_SCC_SWEEP_TOTAL_MS, sample);
        break;
      case JS_TELEMETRY_GC_SCC_SWEEP_MAX_PAUSE_MS:
        Telemetry::Accumulate(Telemetry::GC_SCC_SWEEP_MAX_PAUSE_MS, sample);
        break;
      case JS_TELEMETRY_GC_MINOR_REASON:
        Telemetry::Accumulate(Telemetry::GC_MINOR_REASON, sample);
        break;
      case JS_TELEMETRY_GC_MINOR_REASON_LONG:
        Telemetry::Accumulate(Telemetry::GC_MINOR_REASON_LONG, sample);
        break;
      case JS_TELEMETRY_GC_MINOR_US:
        Telemetry::Accumulate(Telemetry::GC_MINOR_US, sample);
        break;
      case JS_TELEMETRY_GC_NURSERY_BYTES:
        Telemetry::Accumulate(Telemetry::GC_NURSERY_BYTES, sample);
        break;
      case JS_TELEMETRY_GC_PRETENURE_COUNT:
        Telemetry::Accumulate(Telemetry::GC_PRETENURE_COUNT, sample);
        break;
      case JS_TELEMETRY_DEPRECATED_LANGUAGE_EXTENSIONS_IN_CONTENT:
        Telemetry::Accumulate(Telemetry::JS_DEPRECATED_LANGUAGE_EXTENSIONS_IN_CONTENT, sample);
        break;
      case JS_TELEMETRY_DEPRECATED_LANGUAGE_EXTENSIONS_IN_ADDONS:
        Telemetry::Accumulate(Telemetry::JS_DEPRECATED_LANGUAGE_EXTENSIONS_IN_ADDONS, sample);
        break;
      case JS_TELEMETRY_ADDON_EXCEPTIONS:
        Telemetry::Accumulate(Telemetry::JS_TELEMETRY_ADDON_EXCEPTIONS, nsDependentCString(key), sample);
        break;
      case JS_TELEMETRY_AOT_USAGE:
        Telemetry::Accumulate(Telemetry::JS_AOT_USAGE, sample);
        break;
      case JS_TELEMETRY_PRIVILEGED_PARSER_COMPILE_LAZY_AFTER_MS:
        Telemetry::Accumulate(Telemetry::JS_PRIVILEGED_PARSER_COMPILE_LAZY_AFTER_MS, sample);
        break;
      case JS_TELEMETRY_WEB_PARSER_COMPILE_LAZY_AFTER_MS:
        Telemetry::Accumulate(Telemetry::JS_WEB_PARSER_COMPILE_LAZY_AFTER_MS, sample);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected JS_TELEMETRY id");
    }
}

static void
CompartmentNameCallback(JSContext* cx, JSCompartment* comp,
                        char* buf, size_t bufsize)
{
    nsCString name;
    // This is called via the JSAPI and isn't involved in memory reporting, so
    // we don't need to anonymize compartment names.
    int anonymizeID = 0;
    GetCompartmentName(comp, name, &anonymizeID, /* replaceSlashes = */ false);
    if (name.Length() >= bufsize)
        name.Truncate(bufsize - 1);
    memcpy(buf, name.get(), name.Length() + 1);
}

static bool
PreserveWrapper(JSContext* cx, JSObject* obj)
{
    MOZ_ASSERT(cx);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(IS_WN_REFLECTOR(obj) || mozilla::dom::IsDOMObject(obj));

    return mozilla::dom::IsDOMObject(obj) && mozilla::dom::TryPreserveWrapper(obj);
}

static nsresult
ReadSourceFromFilename(JSContext* cx, const char* filename, char16_t** src, size_t* len)
{
    nsresult rv;

    // mozJSSubScriptLoader prefixes the filenames of the scripts it loads with
    // the filename of its caller. Axe that if present.
    const char* arrow;
    while ((arrow = strstr(filename, " -> ")))
        filename = arrow + strlen(" -> ");

    // Get the URI.
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), filename);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> scriptChannel;
    rv = NS_NewChannel(getter_AddRefs(scriptChannel),
                       uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER);
    NS_ENSURE_SUCCESS(rv, rv);

    // Only allow local reading.
    nsCOMPtr<nsIURI> actualUri;
    rv = scriptChannel->GetURI(getter_AddRefs(actualUri));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString scheme;
    rv = actualUri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!scheme.EqualsLiteral("file") && !scheme.EqualsLiteral("jar"))
        return NS_OK;

    // Explicitly set the content type so that we don't load the
    // exthandler to guess it.
    scriptChannel->SetContentType(NS_LITERAL_CSTRING("text/plain"));

    nsCOMPtr<nsIInputStream> scriptStream;
    rv = scriptChannel->Open2(getter_AddRefs(scriptStream));
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t rawLen;
    rv = scriptStream->Available(&rawLen);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!rawLen)
        return NS_ERROR_FAILURE;

    // Technically, this should be SIZE_MAX, but we don't run on machines
    // where that would be less than UINT32_MAX, and the latter is already
    // well beyond a reasonable limit.
    if (rawLen > UINT32_MAX)
        return NS_ERROR_FILE_TOO_BIG;

    // Allocate an internal buf the size of the file.
    auto buf = MakeUniqueFallible<unsigned char[]>(rawLen);
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    unsigned char* ptr = buf.get();
    unsigned char* end = ptr + rawLen;
    while (ptr < end) {
        uint32_t bytesRead;
        rv = scriptStream->Read(reinterpret_cast<char*>(ptr), end - ptr, &bytesRead);
        if (NS_FAILED(rv))
            return rv;
        MOZ_ASSERT(bytesRead > 0, "stream promised more bytes before EOF");
        ptr += bytesRead;
    }

    rv = ScriptLoader::ConvertToUTF16(scriptChannel, buf.get(), rawLen,
                                      EmptyString(), nullptr, *src, *len);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!*src)
        return NS_ERROR_FAILURE;

    // Historically this method used JS_malloc() which updates the GC memory
    // accounting.  Since ConvertToUTF16() now uses js_malloc() instead we
    // update the accounting manually after the fact.
    JS_updateMallocCounter(cx, *len);

    return NS_OK;
}

// The JS engine calls this object's 'load' member function when it needs
// the source for a chrome JS function. See the comment in the XPCJSRuntime
// constructor.
class XPCJSSourceHook: public js::SourceHook {
    bool load(JSContext* cx, const char* filename, char16_t** src, size_t* length) {
        *src = nullptr;
        *length = 0;

        if (!nsContentUtils::IsSystemCaller(cx))
            return true;

        if (!filename)
            return true;

        nsresult rv = ReadSourceFromFilename(cx, filename, src, length);
        if (NS_FAILED(rv)) {
            xpc::Throw(cx, rv);
            return false;
        }

        return true;
    }
};

static const JSWrapObjectCallbacks WrapObjectCallbacks = {
    xpc::WrapperFactory::Rewrap,
    xpc::WrapperFactory::PrepareForWrapping
};

XPCJSRuntime::XPCJSRuntime(JSContext* aCx)
 : CycleCollectedJSRuntime(aCx),
   mWrappedJSMap(JSObject2WrappedJSMap::newMap(XPC_JS_MAP_LENGTH)),
   mWrappedJSClassMap(IID2WrappedJSClassMap::newMap(XPC_JS_CLASS_MAP_LENGTH)),
   mIID2NativeInterfaceMap(IID2NativeInterfaceMap::newMap(XPC_NATIVE_INTERFACE_MAP_LENGTH)),
   mClassInfo2NativeSetMap(ClassInfo2NativeSetMap::newMap(XPC_NATIVE_SET_MAP_LENGTH)),
   mNativeSetMap(NativeSetMap::newMap(XPC_NATIVE_SET_MAP_LENGTH)),
   mThisTranslatorMap(IID2ThisTranslatorMap::newMap(XPC_THIS_TRANSLATOR_MAP_LENGTH)),
   mDyingWrappedNativeProtoMap(XPCWrappedNativeProtoMap::newMap(XPC_DYING_NATIVE_PROTO_MAP_LENGTH)),
   mGCIsRunning(false),
   mNativesToReleaseArray(),
   mDoingFinalization(false),
   mVariantRoots(nullptr),
   mWrappedJSRoots(nullptr),
   mAsyncSnowWhiteFreer(new AsyncFreeSnowWhite())
{
    MOZ_COUNT_CTOR_INHERITED(XPCJSRuntime, CycleCollectedJSRuntime);
}

/* static */
XPCJSRuntime*
XPCJSRuntime::Get()
{
    return nsXPConnect::GetRuntimeInstance();
}

void
XPCJSRuntime::Initialize(JSContext* cx)
{
    mUnprivilegedJunkScope.init(cx, nullptr);
    mPrivilegedJunkScope.init(cx, nullptr);
    mCompilationScope.init(cx, nullptr);

    // these jsids filled in later when we have a JSContext to work with.
    mStrIDs[0] = JSID_VOID;

    // Unconstrain the runtime's threshold on nominal heap size, to avoid
    // triggering GC too often if operating continuously near an arbitrary
    // finite threshold (0xffffffff is infinity for uint32_t parameters).
    // This leaves the maximum-JS_malloc-bytes threshold still in effect
    // to cause period, and we hope hygienic, last-ditch GCs from within
    // the GC's allocator.
    JS_SetGCParameter(cx, JSGC_MAX_BYTES, 0xffffffff);

    JS_SetDestroyCompartmentCallback(cx, CompartmentDestroyedCallback);
    JS_SetSizeOfIncludingThisCompartmentCallback(cx, CompartmentSizeOfIncludingThisCallback);
    JS_SetCompartmentNameCallback(cx, CompartmentNameCallback);
    mPrevGCSliceCallback = JS::SetGCSliceCallback(cx, GCSliceCallback);
    mPrevDoCycleCollectionCallback = JS::SetDoCycleCollectionCallback(cx,
            DoCycleCollectionCallback);
    JS_AddFinalizeCallback(cx, FinalizeCallback, nullptr);
    JS_AddWeakPointerZonesCallback(cx, WeakPointerZonesCallback, this);
    JS_AddWeakPointerCompartmentCallback(cx, WeakPointerCompartmentCallback, this);
    JS_SetWrapObjectCallbacks(cx, &WrapObjectCallbacks);
    js::SetPreserveWrapperCallback(cx, PreserveWrapper);
    JS_SetAccumulateTelemetryCallback(cx, AccumulateTelemetryCallback);
    js::SetWindowProxyClass(cx, &OuterWindowProxyClass);
    js::SetXrayJitInfo(&gXrayJitInfo);
    JS::SetProcessLargeAllocationFailureCallback(OnLargeAllocationFailureCallback);

    // The JS engine needs to keep the source code around in order to implement
    // Function.prototype.toSource(). It'd be nice to not have to do this for
    // chrome code and simply stub out requests for source on it. Life is not so
    // easy, unfortunately. Nobody relies on chrome toSource() working in core
    // browser code, but chrome tests use it. The worst offenders are addons,
    // which like to monkeypatch chrome functions by calling toSource() on them
    // and using regular expressions to modify them. We avoid keeping most browser
    // JS source code in memory by setting LAZY_SOURCE on JS::CompileOptions when
    // compiling some chrome code. This causes the JS engine not save the source
    // code in memory. When the JS engine is asked to provide the source for a
    // function compiled with LAZY_SOURCE, it calls SourceHook to load it.
    ///
    // Note we do have to retain the source code in memory for scripts compiled in
    // isRunOnce mode and compiled function bodies (from
    // JS::CompileFunction). In practice, this means content scripts and event
    // handlers.
    mozilla::UniquePtr<XPCJSSourceHook> hook(new XPCJSSourceHook);
    js::SetSourceHook(cx, Move(hook));

    // Register memory reporters and distinguished amount functions.
    RegisterStrongMemoryReporter(new JSMainRuntimeCompartmentsReporter());
    RegisterStrongMemoryReporter(new JSMainRuntimeTemporaryPeakReporter());
    RegisterJSMainRuntimeGCHeapDistinguishedAmount(JSMainRuntimeGCHeapDistinguishedAmount);
    RegisterJSMainRuntimeTemporaryPeakDistinguishedAmount(JSMainRuntimeTemporaryPeakDistinguishedAmount);
    RegisterJSMainRuntimeCompartmentsSystemDistinguishedAmount(JSMainRuntimeCompartmentsSystemDistinguishedAmount);
    RegisterJSMainRuntimeCompartmentsUserDistinguishedAmount(JSMainRuntimeCompartmentsUserDistinguishedAmount);
    mozilla::RegisterJSSizeOfTab(JSSizeOfTab);
}

bool
XPCJSRuntime::InitializeStrings(JSContext* cx)
{
    JSAutoRequest ar(cx);

    // if it is our first context then we need to generate our string ids
    if (JSID_IS_VOID(mStrIDs[0])) {
        RootedString str(cx);
        for (unsigned i = 0; i < XPCJSContext::IDX_TOTAL_COUNT; i++) {
            str = JS_AtomizeAndPinString(cx, mStrings[i]);
            if (!str) {
                mStrIDs[0] = JSID_VOID;
                return false;
            }
            mStrIDs[i] = INTERNED_STRING_TO_JSID(cx, str);
            mStrJSVals[i].setString(str);
        }

        if (!mozilla::dom::DefineStaticJSVals(cx)) {
            return false;
        }
    }

    return true;
}

bool
XPCJSRuntime::DescribeCustomObjects(JSObject* obj, const js::Class* clasp,
                                    char (&name)[72]) const
{

    if (!IS_PROTO_CLASS(clasp)) {
        return false;
    }

    XPCWrappedNativeProto* p =
        static_cast<XPCWrappedNativeProto*>(xpc_GetJSPrivate(obj));
    nsCOMPtr<nsIXPCScriptable> scr = p->GetScriptable();
    if (!scr) {
        return false;
    }

    SprintfLiteral(name, "JS Object (%s - %s)",
                   clasp->name, scr->GetJSClass()->name);
    return true;
}

bool
XPCJSRuntime::NoteCustomGCThingXPCOMChildren(const js::Class* clasp, JSObject* obj,
                                             nsCycleCollectionTraversalCallback& cb) const
{
    if (clasp != &XPC_WN_Tearoff_JSClass) {
        return false;
    }

    // A tearoff holds a strong reference to its native object
    // (see XPCWrappedNative::FlatJSObjectFinalized). Its XPCWrappedNative
    // will be held alive through the parent of the JSObject of the tearoff.
    XPCWrappedNativeTearOff* to =
        static_cast<XPCWrappedNativeTearOff*>(xpc_GetJSPrivate(obj));
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "xpc_GetJSPrivate(obj)->mNative");
    cb.NoteXPCOMChild(to->GetNative());
    return true;
}

/***************************************************************************/

void
XPCJSRuntime::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth--;
    XPC_LOG_ALWAYS(("XPCJSRuntime @ %p", this));
        XPC_LOG_INDENT();

        XPC_LOG_ALWAYS(("mWrappedJSClassMap @ %p with %d wrapperclasses(s)",
                        mWrappedJSClassMap, mWrappedJSClassMap->Count()));
        // iterate wrappersclasses...
        if (depth && mWrappedJSClassMap->Count()) {
            XPC_LOG_INDENT();
            for (auto i = mWrappedJSClassMap->Iter(); !i.Done(); i.Next()) {
                auto entry = static_cast<IID2WrappedJSClassMap::Entry*>(i.Get());
                entry->value->DebugDump(depth);
            }
            XPC_LOG_OUTDENT();
        }

        // iterate wrappers...
        XPC_LOG_ALWAYS(("mWrappedJSMap @ %p with %d wrappers(s)",
                        mWrappedJSMap, mWrappedJSMap->Count()));
        if (depth && mWrappedJSMap->Count()) {
            XPC_LOG_INDENT();
            mWrappedJSMap->Dump(depth);
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_ALWAYS(("mIID2NativeInterfaceMap @ %p with %d interface(s)",
                        mIID2NativeInterfaceMap,
                        mIID2NativeInterfaceMap->Count()));

        XPC_LOG_ALWAYS(("mClassInfo2NativeSetMap @ %p with %d sets(s)",
                        mClassInfo2NativeSetMap,
                        mClassInfo2NativeSetMap->Count()));

        XPC_LOG_ALWAYS(("mThisTranslatorMap @ %p with %d translator(s)",
                        mThisTranslatorMap, mThisTranslatorMap->Count()));

        XPC_LOG_ALWAYS(("mNativeSetMap @ %p with %d sets(s)",
                        mNativeSetMap, mNativeSetMap->Count()));

        // iterate sets...
        if (depth && mNativeSetMap->Count()) {
            XPC_LOG_INDENT();
            for (auto i = mNativeSetMap->Iter(); !i.Done(); i.Next()) {
                auto entry = static_cast<NativeSetMap::Entry*>(i.Get());
                entry->key_value->DebugDump(depth);
            }
            XPC_LOG_OUTDENT();
        }

        XPC_LOG_OUTDENT();
#endif
}

/***************************************************************************/

void
XPCRootSetElem::AddToRootSet(XPCRootSetElem** listHead)
{
    MOZ_ASSERT(!mSelfp, "Must be not linked");

    mSelfp = listHead;
    mNext = *listHead;
    if (mNext) {
        MOZ_ASSERT(mNext->mSelfp == listHead, "Must be list start");
        mNext->mSelfp = &mNext;
    }
    *listHead = this;
}

void
XPCRootSetElem::RemoveFromRootSet()
{
    JS::NotifyGCRootsRemoved(XPCJSContext::Get()->Context());

    MOZ_ASSERT(mSelfp, "Must be linked");

    MOZ_ASSERT(*mSelfp == this, "Link invariant");
    *mSelfp = mNext;
    if (mNext)
        mNext->mSelfp = mSelfp;
#ifdef DEBUG
    mSelfp = nullptr;
    mNext = nullptr;
#endif
}

void
XPCJSRuntime::AddGCCallback(xpcGCCallback cb)
{
    MOZ_ASSERT(cb, "null callback");
    extraGCCallbacks.AppendElement(cb);
}

void
XPCJSRuntime::RemoveGCCallback(xpcGCCallback cb)
{
    MOZ_ASSERT(cb, "null callback");
    bool found = extraGCCallbacks.RemoveElement(cb);
    if (!found) {
        NS_ERROR("Removing a callback which was never added.");
    }
}

void
XPCJSRuntime::InitSingletonScopes()
{
    // This all happens very early, so we don't bother with cx pushing.
    JSContext* cx = XPCJSContext::Get()->Context();
    JSAutoRequest ar(cx);
    RootedValue v(cx);
    nsresult rv;

    // Create the Unprivileged Junk Scope.
    SandboxOptions unprivilegedJunkScopeOptions;
    unprivilegedJunkScopeOptions.sandboxName.AssignLiteral("XPConnect Junk Compartment");
    unprivilegedJunkScopeOptions.invisibleToDebugger = true;
    rv = CreateSandboxObject(cx, &v, nullptr, unprivilegedJunkScopeOptions);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    mUnprivilegedJunkScope = js::UncheckedUnwrap(&v.toObject());

    // Create the Privileged Junk Scope.
    SandboxOptions privilegedJunkScopeOptions;
    privilegedJunkScopeOptions.sandboxName.AssignLiteral("XPConnect Privileged Junk Compartment");
    privilegedJunkScopeOptions.invisibleToDebugger = true;
    privilegedJunkScopeOptions.wantComponents = false;
    rv = CreateSandboxObject(cx, &v, nsXPConnect::SystemPrincipal(), privilegedJunkScopeOptions);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    mPrivilegedJunkScope = js::UncheckedUnwrap(&v.toObject());

    // Create the Compilation Scope.
    SandboxOptions compilationScopeOptions;
    compilationScopeOptions.sandboxName.AssignLiteral("XPConnect Compilation Compartment");
    compilationScopeOptions.invisibleToDebugger = true;
    compilationScopeOptions.discardSource = ShouldDiscardSystemSource();
    rv = CreateSandboxObject(cx, &v, /* principal = */ nullptr, compilationScopeOptions);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    mCompilationScope = js::UncheckedUnwrap(&v.toObject());
}

void
XPCJSRuntime::DeleteSingletonScopes()
{
    mUnprivilegedJunkScope = nullptr;
    mPrivilegedJunkScope = nullptr;
    mCompilationScope = nullptr;
}
