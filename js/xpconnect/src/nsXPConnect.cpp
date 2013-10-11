/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* High level class and public functions implementation. */

#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/Likely.h"

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"
#include "nsJSEnvironment.h"
#include "nsThreadUtils.h"
#include "nsDOMJSUtils.h"

#include "WrapperFactory.h"
#include "AccessCheck.h"

#ifdef MOZ_JSDEBUGGER
#include "jsdIDebuggerService.h"
#endif

#include "XPCQuickStubs.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/DOMErrorBinding.h"

#include "nsDOMMutationObserver.h"
#include "nsICycleCollectorListener.h"
#include "nsThread.h"
#include "mozilla/XPTInterfaceInfoManager.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace xpc;
using namespace JS;

using mozilla::dom::indexedDB::IndexedDatabaseManager;

NS_IMPL_ISUPPORTS5(nsXPConnect,
                   nsIXPConnect,
                   nsISupportsWeakReference,
                   nsIThreadObserver,
                   nsIJSRuntimeService,
                   nsIJSEngineTelemetryStats)

nsXPConnect* nsXPConnect::gSelf = nullptr;
bool         nsXPConnect::gOnceAliveNowDead = false;
uint32_t     nsXPConnect::gReportAllJSExceptions = 0;

bool         xpc::gDebugMode = false;
bool         xpc::gDesiredDebugMode = false;

// Global cache of the default script security manager (QI'd to
// nsIScriptSecurityManager)
nsIScriptSecurityManager *nsXPConnect::gScriptSecurityManager = nullptr;

const char XPC_CONTEXT_STACK_CONTRACTID[] = "@mozilla.org/js/xpc/ContextStack;1";
const char XPC_RUNTIME_CONTRACTID[]       = "@mozilla.org/js/xpc/RuntimeService;1";
const char XPC_EXCEPTION_CONTRACTID[]     = "@mozilla.org/js/xpc/Exception;1";
const char XPC_CONSOLE_CONTRACTID[]       = "@mozilla.org/consoleservice;1";
const char XPC_SCRIPT_ERROR_CONTRACTID[]  = "@mozilla.org/scripterror;1";
const char XPC_ID_CONTRACTID[]            = "@mozilla.org/js/xpc/ID;1";
const char XPC_XPCONNECT_CONTRACTID[]     = "@mozilla.org/js/xpc/XPConnect;1";

/***************************************************************************/

nsXPConnect::nsXPConnect()
    :   mRuntime(nullptr),
        mDefaultSecurityManager(nullptr),
        mShuttingDown(false),
        mEventDepth(0)
{
    mRuntime = XPCJSRuntime::newXPCJSRuntime(this);

    char* reportableEnv = PR_GetEnv("MOZ_REPORT_ALL_JS_EXCEPTIONS");
    if (reportableEnv && *reportableEnv)
        gReportAllJSExceptions = 1;
}

nsXPConnect::~nsXPConnect()
{
    mRuntime->DeleteJunkScope();
    mRuntime->DestroyJSContextStack();

    // In order to clean up everything properly, we need to GC twice: once now,
    // to clean anything that can go away on its own (like the Junk Scope, which
    // we unrooted above), and once after forcing a bunch of shutdown in
    // XPConnect, to clean the stuff we forcibly disconnected. The forced
    // shutdown code defaults to leaking in a number of situations, so we can't
    // get by with only the second GC. :-(
    JS_GC(mRuntime->Runtime());

    mShuttingDown = true;
    XPCWrappedNativeScope::SystemIsBeingShutDown();
    mRuntime->SystemIsBeingShutDown();

    // The above causes us to clean up a bunch of XPConnect data structures,
    // after which point we need to GC to clean everything up. We need to do
    // this before deleting the XPCJSRuntime, because doing so destroys the
    // maps that our finalize callback depends on.
    JS_GC(mRuntime->Runtime());

    NS_IF_RELEASE(mDefaultSecurityManager);
    gScriptSecurityManager = nullptr;

    // shutdown the logging system
    XPC_LOG_FINISH();

    delete mRuntime;

    gSelf = nullptr;
    gOnceAliveNowDead = true;
}

// static
void
nsXPConnect::InitStatics()
{
    gSelf = new nsXPConnect();
    gOnceAliveNowDead = false;
    if (!gSelf->mRuntime) {
        NS_RUNTIMEABORT("Couldn't create XPCJSRuntime.");
    }

    // Initial extra ref to keep the singleton alive
    // balanced by explicit call to ReleaseXPConnectSingleton()
    NS_ADDREF(gSelf);

    // Set XPConnect as the main thread observer.
    if (NS_FAILED(nsThread::SetMainThreadObserver(gSelf))) {
        MOZ_CRASH();
    }
}

nsXPConnect*
nsXPConnect::GetSingleton()
{
    nsXPConnect* xpc = nsXPConnect::XPConnect();
    NS_IF_ADDREF(xpc);
    return xpc;
}

// static
void
nsXPConnect::ReleaseXPConnectSingleton()
{
    nsXPConnect* xpc = gSelf;
    if (xpc) {
        nsThread::SetMainThreadObserver(nullptr);

#ifdef DEBUG
        // force a dump of the JavaScript gc heap if JS is still alive
        // if requested through XPC_SHUTDOWN_HEAP_DUMP environment variable
        {
            const char* dumpName = getenv("XPC_SHUTDOWN_HEAP_DUMP");
            if (dumpName) {
                FILE* dumpFile = (*dumpName == '\0' ||
                                  strcmp(dumpName, "stdout") == 0)
                                 ? stdout
                                 : fopen(dumpName, "w");
                if (dumpFile) {
                    JS_DumpHeap(xpc->GetRuntime()->Runtime(), dumpFile, nullptr,
                                JSTRACE_OBJECT, nullptr, static_cast<size_t>(-1), nullptr);
                    if (dumpFile != stdout)
                        fclose(dumpFile);
                }
            }
        }
#endif
#ifdef XPC_DUMP_AT_SHUTDOWN
        // NOTE: to see really interesting stuff turn on the prlog stuff.
        // See the comment at the top of XPCLog.h to see how to do that.
        xpc->DebugDump(7);
#endif
        nsrefcnt cnt;
        NS_RELEASE2(xpc, cnt);
#ifdef XPC_DUMP_AT_SHUTDOWN
        if (0 != cnt)
            printf("*** dangling reference to nsXPConnect: refcnt=%d\n", cnt);
        else
            printf("+++ XPConnect had no dangling references.\n");
#endif
    }
}

// static
XPCJSRuntime*
nsXPConnect::GetRuntimeInstance()
{
    nsXPConnect* xpc = XPConnect();
    return xpc->GetRuntime();
}

// static
bool
nsXPConnect::IsISupportsDescendant(nsIInterfaceInfo* info)
{
    bool found = false;
    if (info)
        info->HasAncestor(&NS_GET_IID(nsISupports), &found);
    return found;
}

void
xpc::SystemErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    // It would be nice to assert !JS_DescribeScriptedCaller here, to be sure
    // that there isn't any script running that could catch the exception. But
    // the JS engine invokes the error reporter directly if someone reports an
    // ErrorReport that it doesn't know how to turn into an exception. Arguably
    // it should just learn how to throw everything. But either way, if the
    // exception is ending here, it's not going to get propagated to a caller,
    // so it's up to us to make it known.

    nsresult rv;

    /* Use the console service to register the error. */
    nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);

    /*
     * Make an nsIScriptError, populate it with information from this
     * error, then log it with the console service.
     */
    nsCOMPtr<nsIScriptError> errorObject =
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);

    if (consoleService && errorObject) {
        uint32_t column = rep->uctokenptr - rep->uclinebuf;

        const PRUnichar* ucmessage =
            static_cast<const PRUnichar*>(rep->ucmessage);
        const PRUnichar* uclinebuf =
            static_cast<const PRUnichar*>(rep->uclinebuf);

        rv = errorObject->Init(
              ucmessage ? nsDependentString(ucmessage) : EmptyString(),
              NS_ConvertASCIItoUTF16(rep->filename),
              uclinebuf ? nsDependentString(uclinebuf) : EmptyString(),
              rep->lineno, column, rep->flags,
              "system javascript");
        if (NS_SUCCEEDED(rv))
            consoleService->LogMessage(errorObject);
    }

    if (nsContentUtils::DOMWindowDumpEnabled()) {
        fprintf(stderr, "System JS : %s %s:%d\n"
                "                     %s\n",
                JSREPORT_IS_WARNING(rep->flags) ? "WARNING" : "ERROR",
                rep->filename, rep->lineno,
                message ? message : "<no message>");
    }

}

NS_EXPORT_(void)
xpc::SystemErrorReporterExternal(JSContext *cx, const char *message,
                                 JSErrorReport *rep)
{
    return SystemErrorReporter(cx, message, rep);
}


/***************************************************************************/


nsresult
nsXPConnect::GetInfoForIID(const nsIID * aIID, nsIInterfaceInfo** info)
{
  return XPTInterfaceInfoManager::GetSingleton()->GetInfoForIID(aIID, info);
}

nsresult
nsXPConnect::GetInfoForName(const char * name, nsIInterfaceInfo** info)
{
  nsresult rv = XPTInterfaceInfoManager::GetSingleton()->GetInfoForName(name, info);
  return NS_FAILED(rv) ? NS_OK : NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsXPConnect::GarbageCollect(uint32_t reason)
{
    GetRuntime()->Collect(reason);
    return NS_OK;
}

bool
xpc_GCThingIsGrayCCThing(void *thing)
{
    return AddToCCKind(js::GCThingTraceKind(thing)) &&
           xpc_IsGrayGCThing(thing);
}

void
xpc_MarkInCCGeneration(nsISupports* aVariant, uint32_t aGeneration)
{
    nsCOMPtr<XPCVariant> variant = do_QueryInterface(aVariant);
    if (variant) {
        variant->SetCCGeneration(aGeneration);
        variant->GetJSVal(); // Unmarks gray JSObject.
        XPCVariant* weak = variant.get();
        variant = nullptr;
        if (weak->IsPurple()) {
          weak->RemovePurple();
        }
    }
}

void
xpc_TryUnmarkWrappedGrayObject(nsISupports* aWrappedJS)
{
    nsCOMPtr<nsIXPConnectWrappedJS> wjs = do_QueryInterface(aWrappedJS);
    if (wjs) {
        // Unmarks gray JSObject.
        static_cast<nsXPCWrappedJS*>(wjs.get())->GetJSObject();
    }
}

/***************************************************************************/
/***************************************************************************/
// nsIXPConnect interface methods...

static inline nsresult UnexpectedFailure(nsresult rv)
{
    NS_ERROR("This is not supposed to fail!");
    return rv;
}

/* void initClasses (in JSContextPtr aJSContext, in JSObjectPtr aGlobalJSObj); */
NS_IMETHODIMP
nsXPConnect::InitClasses(JSContext * aJSContext, JSObject * aGlobalJSObj)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aGlobalJSObj, "bad param");
    RootedObject globalJSObj(aJSContext, aGlobalJSObj);

    JSAutoCompartment ac(aJSContext, globalJSObj);

    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::GetNewOrUsed(aJSContext, globalJSObj);

    if (!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    scope->RemoveWrappedNativeProtos();

    if (!XPCNativeWrapper::AttachNewConstructorObject(aJSContext, globalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    return NS_OK;
}

#ifdef DEBUG
struct VerifyTraceXPCGlobalCalledTracer
{
    JSTracer base;
    bool ok;
};

static void
VerifyTraceXPCGlobalCalled(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    // We don't do anything here, we only want to verify that TraceXPCGlobal
    // was called.
}
#endif

void
TraceXPCGlobal(JSTracer *trc, JSObject *obj)
{
#ifdef DEBUG
    if (trc->callback == VerifyTraceXPCGlobalCalled) {
        // We don't do anything here, we only want to verify that TraceXPCGlobal
        // was called.
        reinterpret_cast<VerifyTraceXPCGlobalCalledTracer*>(trc)->ok = true;
        return;
    }
#endif

    if (js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL)
        mozilla::dom::TraceProtoAndIfaceCache(trc, obj);
}

#ifdef DEBUG
#include "mozilla/Preferences.h"
#include "nsIXULRuntime.h"
static void
CheckTypeInference(JSContext *cx, const JSClass *clasp, nsIPrincipal *principal)
{
    // Check that the global class isn't whitelisted.
    if (strcmp(clasp->name, "Sandbox") ||
        strcmp(clasp->name, "nsXBLPrototypeScript compilation scope") ||
        strcmp(clasp->name, "nsXULPrototypeScript compilation scope"))
        return;

    // Check that the pref is on.
    if (!mozilla::Preferences::GetBool("javascript.options.typeinference"))
        return;

    // Check that we're not chrome.
    bool isSystem;
    nsIScriptSecurityManager* ssm;
    ssm = XPCWrapper::GetSecurityManager();
    if (NS_FAILED(ssm->IsSystemPrincipal(principal, &isSystem)) || !isSystem)
        return;

    // Check that safe mode isn't on.
    bool safeMode;
    nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
    if (!xr) {
        NS_WARNING("Couldn't get XUL runtime!");
        return;
    }
    if (NS_FAILED(xr->GetInSafeMode(&safeMode)) || safeMode)
        return;

    // Finally, do the damn assert.
    MOZ_ASSERT(JS_GetOptions(cx) & JSOPTION_TYPE_INFERENCE);
}
#else
#define CheckTypeInference(cx, clasp, principal) {}
#endif

namespace xpc {

JSObject*
CreateGlobalObject(JSContext *cx, const JSClass *clasp, nsIPrincipal *principal,
                   JS::CompartmentOptions& aOptions)
{
    // Make sure that Type Inference is enabled for everything non-chrome.
    // Sandboxes and compilation scopes are exceptions. See bug 744034.
    CheckTypeInference(cx, clasp, principal);

    MOZ_ASSERT(NS_IsMainThread(), "using a principal off the main thread?");
    MOZ_ASSERT(principal);

    RootedObject global(cx,
                        JS_NewGlobalObject(cx, clasp, nsJSPrincipals::get(principal),
                                           JS::DontFireOnNewGlobalHook, aOptions));
    if (!global)
        return nullptr;
    JSAutoCompartment ac(cx, global);
    // The constructor automatically attaches the scope to the compartment private
    // of |global|.
    (void) new XPCWrappedNativeScope(cx, global);

#ifdef DEBUG
    // Verify that the right trace hook is called. Note that this doesn't
    // work right for wrapped globals, since the tracing situation there is
    // more complicated. Manual inspection shows that they do the right thing.
    if (!((const js::Class*)clasp)->ext.isWrappedNative)
    {
        VerifyTraceXPCGlobalCalledTracer trc;
        JS_TracerInit(&trc.base, JS_GetRuntime(cx), VerifyTraceXPCGlobalCalled);
        trc.ok = false;
        JS_TraceChildren(&trc.base, global, JSTRACE_OBJECT);
        MOZ_ASSERT(trc.ok, "Trace hook on global needs to call TraceXPCGlobal for XPConnect compartments.");
    }
#endif

    if (clasp->flags & JSCLASS_DOM_GLOBAL) {
        AllocateProtoAndIfaceCache(global);
    }

    return global;
}

} // namespace xpc

NS_IMETHODIMP
nsXPConnect::InitClassesWithNewWrappedGlobal(JSContext * aJSContext,
                                             nsISupports *aCOMObj,
                                             nsIPrincipal * aPrincipal,
                                             uint32_t aFlags,
                                             JS::CompartmentOptions& aOptions,
                                             nsIXPConnectJSObjectHolder **_retval)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aCOMObj, "bad param");
    MOZ_ASSERT(_retval, "bad param");

    // We pass null for the 'extra' pointer during global object creation, so
    // we need to have a principal.
    MOZ_ASSERT(aPrincipal);

    // Call into XPCWrappedNative to make a new global object, scope, and global
    // prototype.
    xpcObjectHelper helper(aCOMObj);
    MOZ_ASSERT(helper.GetScriptableFlags() & nsIXPCScriptable::IS_GLOBAL_OBJECT);
    nsRefPtr<XPCWrappedNative> wrappedGlobal;
    nsresult rv =
        XPCWrappedNative::WrapNewGlobal(helper, aPrincipal,
                                        aFlags & nsIXPConnect::INIT_JS_STANDARD_CLASSES,
                                        aOptions, getter_AddRefs(wrappedGlobal));
    NS_ENSURE_SUCCESS(rv, rv);

    // Grab a copy of the global and enter its compartment.
    RootedObject global(aJSContext, wrappedGlobal->GetFlatJSObject());
    MOZ_ASSERT(!js::GetObjectParent(global));
    JSAutoCompartment ac(aJSContext, global);

    if (!(aFlags & nsIXPConnect::OMIT_COMPONENTS_OBJECT)) {
        // XPCCallContext gives us an active request needed to save/restore.
        if (!nsXPCComponents::AttachComponentsObject(aJSContext, wrappedGlobal->GetScope()))
            return UnexpectedFailure(NS_ERROR_FAILURE);

        if (!XPCNativeWrapper::AttachNewConstructorObject(aJSContext, global))
            return UnexpectedFailure(NS_ERROR_FAILURE);
    }

    // Stuff coming through this path always ends up as a DOM global.
    // XXX Someone who knows why we can assert this should re-check
    //     (after bug 720580).
    MOZ_ASSERT(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL);

    // Init WebIDL binding constructors wanted on all XPConnect globals.
    // Additional bindings may be created lazily, see BackstagePass::NewResolve.
    //
    // XXX Please do not add any additional classes here without the approval of
    //     the XPConnect module owner.
    if (!TextDecoderBinding::GetConstructorObject(aJSContext, global) ||
        !TextEncoderBinding::GetConstructorObject(aJSContext, global) ||
        !DOMErrorBinding::GetConstructorObject(aJSContext, global)) {
        return UnexpectedFailure(NS_ERROR_FAILURE);
    }

    if (nsContentUtils::IsSystemPrincipal(aPrincipal) &&
        !IndexedDatabaseManager::DefineIndexedDBLazyGetter(aJSContext,
                                                           global)) {
        return UnexpectedFailure(NS_ERROR_FAILURE);
    }

    wrappedGlobal.forget(_retval);
    return NS_OK;
}

static nsresult
NativeInterface2JSObject(HandleObject aScope,
                         nsISupports *aCOMObj,
                         nsWrapperCache *aCache,
                         const nsIID * aIID,
                         bool aAllowWrapping,
                         jsval *aVal,
                         nsIXPConnectJSObjectHolder **aHolder)
{
    AutoJSContext cx;
    JSAutoCompartment ac(cx, aScope);

    nsresult rv;
    xpcObjectHelper helper(aCOMObj, aCache);
    if (!XPCConvert::NativeInterface2JSObject(aVal, aHolder, helper, aIID,
                                              nullptr, aAllowWrapping, &rv))
        return rv;

    MOZ_ASSERT(aAllowWrapping || !xpc::WrapperFactory::IsXrayWrapper(JSVAL_TO_OBJECT(*aVal)),
               "Shouldn't be returning a xray wrapper here");

    return NS_OK;
}

/* nsIXPConnectJSObjectHolder wrapNative (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsISupports aCOMObj, in nsIIDRef aIID); */
NS_IMETHODIMP
nsXPConnect::WrapNative(JSContext * aJSContext,
                        JSObject * aScopeArg,
                        nsISupports *aCOMObj,
                        const nsIID & aIID,
                        nsIXPConnectJSObjectHolder **aHolder)
{
    MOZ_ASSERT(aHolder, "bad param");
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aScopeArg, "bad param");
    MOZ_ASSERT(aCOMObj, "bad param");

    RootedObject aScope(aJSContext, aScopeArg);
    RootedValue v(aJSContext);
    return NativeInterface2JSObject(aScope, aCOMObj, nullptr, &aIID,
                                    false, v.address(), aHolder);
}

/* void wrapNativeToJSVal (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsISupports aCOMObj, in nsIIDPtr aIID, out jsval aVal, out nsIXPConnectJSObjectHolder aHolder); */
NS_IMETHODIMP
nsXPConnect::WrapNativeToJSVal(JSContext * aJSContext,
                               JSObject * aScopeArg,
                               nsISupports *aCOMObj,
                               nsWrapperCache *aCache,
                               const nsIID * aIID,
                               bool aAllowWrapping,
                               jsval *aVal,
                               nsIXPConnectJSObjectHolder **aHolder)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aScopeArg, "bad param");
    MOZ_ASSERT(aCOMObj, "bad param");

    if (aHolder)
        *aHolder = nullptr;

    RootedObject aScope(aJSContext, aScopeArg);

    return NativeInterface2JSObject(aScope, aCOMObj, aCache, aIID,
                                    aAllowWrapping, aVal, aHolder);
}

/* void wrapJS (in JSContextPtr aJSContext, in JSObjectPtr aJSObj, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP
nsXPConnect::WrapJS(JSContext * aJSContext,
                    JSObject * aJSObjArg,
                    const nsIID & aIID,
                    void * *result)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aJSObjArg, "bad param");
    MOZ_ASSERT(result, "bad param");

    *result = nullptr;

    RootedObject aJSObj(aJSContext, aJSObjArg);
    JSAutoCompartment ac(aJSContext, aJSObj);

    nsresult rv = NS_ERROR_UNEXPECTED;
    if (!XPCConvert::JSObject2NativeInterface(result, aJSObj,
                                              &aIID, nullptr, &rv))
        return rv;
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::JSValToVariant(JSContext *cx,
                            jsval *aJSVal,
                            nsIVariant ** aResult)
{
    NS_PRECONDITION(aJSVal, "bad param");
    NS_PRECONDITION(aResult, "bad param");

    *aResult = XPCVariant::newVariant(cx, *aJSVal);
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

/* void wrapJSAggregatedToNative (in nsISupports aOuter, in JSContextPtr aJSContext, in JSObjectPtr aJSObj, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP
nsXPConnect::WrapJSAggregatedToNative(nsISupports *aOuter,
                                      JSContext * aJSContext,
                                      JSObject * aJSObjArg,
                                      const nsIID & aIID,
                                      void * *result)
{
    MOZ_ASSERT(aOuter, "bad param");
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aJSObjArg, "bad param");
    MOZ_ASSERT(result, "bad param");

    *result = nullptr;

    RootedObject aJSObj(aJSContext, aJSObjArg);
    nsresult rv;
    if (!XPCConvert::JSObject2NativeInterface(result, aJSObj,
                                              &aIID, aOuter, &rv))
        return rv;
    return NS_OK;
}

/* nsIXPConnectWrappedNative getWrappedNativeOfJSObject (in JSContextPtr aJSContext, in JSObjectPtr aJSObj); */
NS_IMETHODIMP
nsXPConnect::GetWrappedNativeOfJSObject(JSContext * aJSContext,
                                        JSObject * aJSObjArg,
                                        nsIXPConnectWrappedNative **_retval)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aJSObjArg, "bad param");
    MOZ_ASSERT(_retval, "bad param");

    RootedObject aJSObj(aJSContext, aJSObjArg);
    aJSObj = js::CheckedUnwrap(aJSObj, /* stopAtOuter = */ false);
    if (aJSObj && IS_WN_REFLECTOR(aJSObj)) {
        NS_IF_ADDREF(*_retval = XPCWrappedNative::Get(aJSObj));
        return NS_OK;
    }

    // else...
    *_retval = nullptr;
    return NS_ERROR_FAILURE;
}

/* nsISupports getNativeOfWrapper(in JSContextPtr aJSContext, in JSObjectPtr  aJSObj); */
NS_IMETHODIMP_(nsISupports*)
nsXPConnect::GetNativeOfWrapper(JSContext * aJSContext,
                                JSObject * aJSObj)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aJSObj, "bad param");

    aJSObj = js::CheckedUnwrap(aJSObj, /* stopAtOuter = */ false);
    if (!aJSObj) {
        JS_ReportError(aJSContext, "Permission denied to get native of security wrapper");
        return nullptr;
    }
    if (IS_WN_REFLECTOR(aJSObj)) {
        if (XPCWrappedNative *wn = XPCWrappedNative::Get(aJSObj))
            return wn->Native();
        return nullptr;
    }

    nsCOMPtr<nsISupports> canonical =
        do_QueryInterface(mozilla::dom::UnwrapDOMObjectToISupports(aJSObj));
    return canonical;
}

/* nsIXPConnectWrappedNative getWrappedNativeOfNativeObject (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsISupports aCOMObj, in nsIIDRef aIID); */
NS_IMETHODIMP
nsXPConnect::GetWrappedNativeOfNativeObject(JSContext * aJSContext,
                                            JSObject * aScopeArg,
                                            nsISupports *aCOMObj,
                                            const nsIID & aIID,
                                            nsIXPConnectWrappedNative **_retval)
{
    MOZ_ASSERT(aJSContext, "bad param");
    MOZ_ASSERT(aScopeArg, "bad param");
    MOZ_ASSERT(aCOMObj, "bad param");
    MOZ_ASSERT(_retval, "bad param");

    *_retval = nullptr;

    RootedObject aScope(aJSContext, aScopeArg);

    XPCWrappedNativeScope* scope = GetObjectScope(aScope);
    if (!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    AutoMarkingNativeInterfacePtr iface(aJSContext);
    iface = XPCNativeInterface::GetNewOrUsed(&aIID);
    if (!iface)
        return NS_ERROR_FAILURE;

    XPCWrappedNative* wrapper;

    nsresult rv = XPCWrappedNative::GetUsedOnly(aCOMObj, scope, iface, &wrapper);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    *_retval = static_cast<nsIXPConnectWrappedNative*>(wrapper);
    return NS_OK;
}

/* void reparentWrappedNativeIfFound (in JSContextPtr aJSContext,
 *                                    in JSObjectPtr aScope,
 *                                    in JSObjectPtr aNewParent,
 *                                    in nsISupports aCOMObj); */
NS_IMETHODIMP
nsXPConnect::ReparentWrappedNativeIfFound(JSContext * aJSContext,
                                          JSObject * aScopeArg,
                                          JSObject * aNewParentArg,
                                          nsISupports *aCOMObj)
{
    RootedObject aScope(aJSContext, aScopeArg);
    RootedObject aNewParent(aJSContext, aNewParentArg);

    XPCWrappedNativeScope* scope = GetObjectScope(aScope);
    XPCWrappedNativeScope* scope2 = GetObjectScope(aNewParent);
    if (!scope || !scope2)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    RootedObject newParent(aJSContext, aNewParent);
    return XPCWrappedNative::
        ReparentWrapperIfFound(scope, scope2, newParent, aCOMObj);
}

static PLDHashOperator
MoveableWrapperFinder(PLDHashTable *table, PLDHashEntryHdr *hdr,
                      uint32_t number, void *arg)
{
    nsTArray<nsRefPtr<XPCWrappedNative> > *array =
        static_cast<nsTArray<nsRefPtr<XPCWrappedNative> > *>(arg);
    XPCWrappedNative *wn = ((Native2WrappedNativeMap::Entry*)hdr)->value;

    // If a wrapper is expired, then there are no references to it from JS, so
    // we don't have to move it.
    if (!wn->IsWrapperExpired())
        array->AppendElement(wn);
    return PL_DHASH_NEXT;
}

/* void rescueOrphansInScope(in JSContextPtr aJSContext, in JSObjectPtr  aScope); */
NS_IMETHODIMP
nsXPConnect::RescueOrphansInScope(JSContext *aJSContext, JSObject *aScopeArg)
{
    RootedObject aScope(aJSContext, aScopeArg);

    XPCWrappedNativeScope *scope = GetObjectScope(aScope);
    if (!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    // First, look through the old scope and find all of the wrappers that we
    // might need to rescue.
    nsTArray<nsRefPtr<XPCWrappedNative> > wrappersToMove;

    {   // scoped lock
        XPCAutoLock lock(GetRuntime()->GetMapLock());
        Native2WrappedNativeMap *map = scope->GetWrappedNativeMap();
        wrappersToMove.SetCapacity(map->Count());
        map->Enumerate(MoveableWrapperFinder, &wrappersToMove);
    }

    // Now that we have the wrappers, reparent them to the new scope.
    for (uint32_t i = 0, stop = wrappersToMove.Length(); i < stop; ++i) {
        nsresult rv = wrappersToMove[i]->RescueOrphans();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

/* void setDefaultSecurityManager (in nsIXPCSecurityManager aManager); */
NS_IMETHODIMP
nsXPConnect::SetDefaultSecurityManager(nsIXPCSecurityManager *aManager)
{
    NS_IF_ADDREF(aManager);
    NS_IF_RELEASE(mDefaultSecurityManager);
    mDefaultSecurityManager = aManager;

    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_QueryInterface(mDefaultSecurityManager);

    // Remember the result of the above QI for fast access to the
    // script securityt manager.
    gScriptSecurityManager = ssm;

    return NS_OK;
}

/* nsIStackFrame createStackFrameLocation (in uint32_t aLanguage, in string aFilename, in string aFunctionName, in int32_t aLineNumber, in nsIStackFrame aCaller); */
NS_IMETHODIMP
nsXPConnect::CreateStackFrameLocation(uint32_t aLanguage,
                                      const char *aFilename,
                                      const char *aFunctionName,
                                      int32_t aLineNumber,
                                      nsIStackFrame *aCaller,
                                      nsIStackFrame **_retval)
{
    MOZ_ASSERT(_retval, "bad param");

    nsCOMPtr<nsIStackFrame> stackFrame =
        exceptions::CreateStackFrameLocation(aLanguage,
                                             aFilename,
                                             aFunctionName,
                                             aLineNumber,
                                             aCaller);
    stackFrame.forget(_retval);
    return NS_OK;
}

/* readonly attribute nsIStackFrame CurrentJSStack; */
NS_IMETHODIMP
nsXPConnect::GetCurrentJSStack(nsIStackFrame * *aCurrentJSStack)
{
    MOZ_ASSERT(aCurrentJSStack, "bad param");

    nsCOMPtr<nsIStackFrame> currentStack = dom::GetCurrentJSStack();
    currentStack.forget(aCurrentJSStack);

    return NS_OK;
}

/* readonly attribute nsIXPCNativeCallContext CurrentNativeCallContext; */
NS_IMETHODIMP
nsXPConnect::GetCurrentNativeCallContext(nsAXPCNativeCallContext * *aCurrentNativeCallContext)
{
    MOZ_ASSERT(aCurrentNativeCallContext, "bad param");

    *aCurrentNativeCallContext = XPCJSRuntime::Get()->GetCallContext();
    return NS_OK;
}

/* void setFunctionThisTranslator (in nsIIDRef aIID, in nsIXPCFunctionThisTranslator aTranslator); */
NS_IMETHODIMP
nsXPConnect::SetFunctionThisTranslator(const nsIID & aIID,
                                       nsIXPCFunctionThisTranslator *aTranslator)
{
    XPCJSRuntime* rt = GetRuntime();
    IID2ThisTranslatorMap* map = rt->GetThisTranslatorMap();
    {
        XPCAutoLock lock(rt->GetMapLock()); // scoped lock
        map->Add(aIID, aTranslator);
    }
    return NS_OK;
}

/* void clearAllWrappedNativeSecurityPolicies (); */
NS_IMETHODIMP
nsXPConnect::ClearAllWrappedNativeSecurityPolicies()
{
    return XPCWrappedNativeScope::ClearAllWrappedNativeSecurityPolicies();
}

NS_IMETHODIMP
nsXPConnect::CreateSandbox(JSContext *cx, nsIPrincipal *principal,
                           nsIXPConnectJSObjectHolder **_retval)
{
    *_retval = nullptr;

    RootedValue rval(cx, JSVAL_VOID);

    SandboxOptions options(cx);
    nsresult rv = CreateSandboxObject(cx, rval.address(), principal, options);
    MOZ_ASSERT(NS_FAILED(rv) || !JSVAL_IS_PRIMITIVE(rval),
               "Bad return value from xpc_CreateSandboxObject()!");

    if (NS_SUCCEEDED(rv) && !JSVAL_IS_PRIMITIVE(rval)) {
        *_retval = XPCJSObjectHolder::newHolder(JSVAL_TO_OBJECT(rval));
        NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);

        NS_ADDREF(*_retval);
    }

    return rv;
}

NS_IMETHODIMP
nsXPConnect::EvalInSandboxObject(const nsAString& source, const char *filename,
                                 JSContext *cx, JSObject *sandboxArg,
                                 bool returnStringOnly, JS::Value *rvalArg)
{
    if (!sandboxArg)
        return NS_ERROR_INVALID_ARG;

    RootedObject sandbox(cx, sandboxArg);
    RootedValue rval(cx);
    nsresult rv = EvalInSandbox(cx, sandbox, source, filename ? filename :
                                "x-bogus://XPConnect/Sandbox", 1, JSVERSION_DEFAULT,
                                returnStringOnly, &rval);
    NS_ENSURE_SUCCESS(rv, rv);
    *rvalArg = rval;
    return NS_OK;
}

/* nsIXPConnectJSObjectHolder getWrappedNativePrototype (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsIClassInfo aClassInfo); */
NS_IMETHODIMP
nsXPConnect::GetWrappedNativePrototype(JSContext * aJSContext,
                                       JSObject * aScopeArg,
                                       nsIClassInfo *aClassInfo,
                                       nsIXPConnectJSObjectHolder **_retval)
{
    RootedObject aScope(aJSContext, aScopeArg);
    JSAutoCompartment ac(aJSContext, aScope);

    XPCWrappedNativeScope* scope = GetObjectScope(aScope);
    if (!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCNativeScriptableCreateInfo sciProto;
    XPCWrappedNative::GatherProtoScriptableCreateInfo(aClassInfo, sciProto);

    AutoMarkingWrappedNativeProtoPtr proto(aJSContext);
    proto = XPCWrappedNativeProto::GetNewOrUsed(scope, aClassInfo, &sciProto);
    if (!proto)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    nsIXPConnectJSObjectHolder* holder;
    *_retval = holder = XPCJSObjectHolder::newHolder(proto->GetJSProtoObject());
    if (!holder)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    NS_ADDREF(holder);
    return NS_OK;
}

/* void debugDump (in short depth); */
NS_IMETHODIMP
nsXPConnect::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPConnect @ %x with mRefCnt = %d", this, mRefCnt.get()));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("gSelf @ %x", gSelf));
        XPC_LOG_ALWAYS(("gOnceAliveNowDead is %d", (int)gOnceAliveNowDead));
        XPC_LOG_ALWAYS(("mDefaultSecurityManager @ %x", mDefaultSecurityManager));
        if (mRuntime) {
            if (depth)
                mRuntime->DebugDump(depth);
            else
                XPC_LOG_ALWAYS(("XPCJSRuntime @ %x", mRuntime));
        } else
            XPC_LOG_ALWAYS(("mRuntime is null"));
        XPCWrappedNativeScope::DebugDumpAllScopes(depth);
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

/* void debugDumpObject (in nsISupports aCOMObj, in short depth); */
NS_IMETHODIMP
nsXPConnect::DebugDumpObject(nsISupports *p, int16_t depth)
{
#ifdef DEBUG
    if (!depth)
        return NS_OK;
    if (!p) {
        XPC_LOG_ALWAYS(("*** Cound not dump object with NULL address"));
        return NS_OK;
    }

    nsIXPConnect* xpc;
    nsIXPCWrappedJSClass* wjsc;
    nsIXPConnectWrappedNative* wn;
    nsIXPConnectWrappedJS* wjs;

    if (NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnect),
                                       (void**)&xpc))) {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnect..."));
        xpc->DebugDump(depth);
        NS_RELEASE(xpc);
    } else if (NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPCWrappedJSClass),
                                              (void**)&wjsc))) {
        XPC_LOG_ALWAYS(("Dumping a nsIXPCWrappedJSClass..."));
        wjsc->DebugDump(depth);
        NS_RELEASE(wjsc);
    } else if (NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnectWrappedNative),
                                              (void**)&wn))) {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedNative..."));
        wn->DebugDump(depth);
        NS_RELEASE(wn);
    } else if (NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnectWrappedJS),
                                              (void**)&wjs))) {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedJS..."));
        wjs->DebugDump(depth);
        NS_RELEASE(wjs);
    } else
        XPC_LOG_ALWAYS(("*** Could not dump the nsISupports @ %x", p));
#endif
    return NS_OK;
}

/* void debugDumpJSStack (in bool showArgs, in bool showLocals, in bool showThisProps); */
NS_IMETHODIMP
nsXPConnect::DebugDumpJSStack(bool showArgs,
                              bool showLocals,
                              bool showThisProps)
{
    JSContext* cx = GetCurrentJSContext();
    if (!cx)
        printf("there is no JSContext on the nsIThreadJSContextStack!\n");
    else
        xpc_DumpJSStack(cx, showArgs, showLocals, showThisProps);

    return NS_OK;
}

char*
nsXPConnect::DebugPrintJSStack(bool showArgs,
                               bool showLocals,
                               bool showThisProps)
{
    JSContext* cx = GetCurrentJSContext();
    if (!cx)
        printf("there is no JSContext on the nsIThreadJSContextStack!\n");
    else
        return xpc_PrintJSStack(cx, showArgs, showLocals, showThisProps);

    return nullptr;
}

/* void debugDumpEvalInJSStackFrame (in uint32_t aFrameNumber, in string aSourceText); */
NS_IMETHODIMP
nsXPConnect::DebugDumpEvalInJSStackFrame(uint32_t aFrameNumber, const char *aSourceText)
{
    JSContext* cx = GetCurrentJSContext();
    if (!cx)
        printf("there is no JSContext on the nsIThreadJSContextStack!\n");
    else
        xpc_DumpEvalInJSStackFrame(cx, aFrameNumber, aSourceText);

    return NS_OK;
}

/* jsval variantToJS (in JSContextPtr ctx, in JSObjectPtr scope, in nsIVariant value); */
NS_IMETHODIMP
nsXPConnect::VariantToJS(JSContext* ctx, JSObject* scopeArg, nsIVariant* value,
                         jsval* _retval)
{
    NS_PRECONDITION(ctx, "bad param");
    NS_PRECONDITION(scopeArg, "bad param");
    NS_PRECONDITION(value, "bad param");
    NS_PRECONDITION(_retval, "bad param");

    RootedObject scope(ctx, scopeArg);
    MOZ_ASSERT(js::IsObjectInContextCompartment(scope, ctx));

    nsresult rv = NS_OK;
    if (!XPCVariant::VariantDataToJS(value, &rv, _retval)) {
        if (NS_FAILED(rv))
            return rv;

        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/* nsIVariant JSToVariant (in JSContextPtr ctx, in jsval value); */
NS_IMETHODIMP
nsXPConnect::JSToVariant(JSContext* ctx, const jsval &value, nsIVariant** _retval)
{
    NS_PRECONDITION(ctx, "bad param");
    NS_PRECONDITION(_retval, "bad param");

    *_retval = XPCVariant::newVariant(ctx, value);
    if (!(*_retval))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::OnProcessNextEvent(nsIThreadInternal *aThread, bool aMayWait,
                                uint32_t aRecursionDepth)
{
    // Record this event.
    mEventDepth++;

    // Push a null JSContext so that we don't see any script during
    // event processing.
    MOZ_ASSERT(NS_IsMainThread());
    bool ok = PushJSContextNoScriptContext(nullptr);
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::AfterProcessNextEvent(nsIThreadInternal *aThread,
                                   uint32_t aRecursionDepth)
{
    // Watch out for unpaired events during observer registration.
    if (MOZ_UNLIKELY(mEventDepth == 0))
        return NS_OK;
    mEventDepth--;

    // Now that we're back to the event loop, reset the slow script checkpoint.
    mRuntime->OnAfterProcessNextEvent();

    // Call cycle collector occasionally.
    MOZ_ASSERT(NS_IsMainThread());
    nsJSContext::MaybePokeCC();
    nsDOMMutationObserver::HandleMutations();

    PopJSContextNoScriptContext();

    // If the cx stack is empty, that means we're at the an un-nested event
    // loop. This is a good time to make changes to debug mode.
    if (XPCJSRuntime::Get()->GetJSContextStack()->Count() == 0) {
        MOZ_ASSERT(mEventDepth == 0);
        CheckForDebugMode(XPCJSRuntime::Get()->Runtime());
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::OnDispatchedEvent(nsIThreadInternal* aThread)
{
    NS_NOTREACHED("Why tell us?");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsXPConnect::SetReportAllJSExceptions(bool newval)
{
    // Ignore if the environment variable was set.
    if (gReportAllJSExceptions != 1)
        gReportAllJSExceptions = newval ? 2 : 0;

    return NS_OK;
}

/* attribute JSRuntime runtime; */
NS_IMETHODIMP
nsXPConnect::GetRuntime(JSRuntime **runtime)
{
    if (!runtime)
        return NS_ERROR_NULL_POINTER;

    JSRuntime *rt = GetRuntime()->Runtime();
    JS_AbortIfWrongThread(rt);
    *runtime = rt;
    return NS_OK;
}

/* [noscript, notxpcom] void registerGCCallback(in xpcGCCallback func); */
NS_IMETHODIMP_(void)
nsXPConnect::RegisterGCCallback(xpcGCCallback func)
{
    mRuntime->AddGCCallback(func);
}

/* [noscript, notxpcom] void unregisterGCCallback(in xpcGCCallback func); */
NS_IMETHODIMP_(void)
nsXPConnect::UnregisterGCCallback(xpcGCCallback func)
{
    mRuntime->RemoveGCCallback(func);
}

/* [noscript, notxpcom] void registerContextCallback(in xpcContextCallback func); */
NS_IMETHODIMP_(void)
nsXPConnect::RegisterContextCallback(xpcContextCallback func)
{
    mRuntime->AddContextCallback(func);
}

/* [noscript, notxpcom] void unregisterContextCallback(in xpcContextCallback func); */
NS_IMETHODIMP_(void)
nsXPConnect::UnregisterContextCallback(xpcContextCallback func)
{
    mRuntime->RemoveContextCallback(func);
}

#ifdef MOZ_JSDEBUGGER
void
nsXPConnect::CheckForDebugMode(JSRuntime *rt)
{
    if (gDebugMode == gDesiredDebugMode) {
        return;
    }

    // This can happen if a Worker is running, but we don't have the ability to
    // debug workers right now, so just return.
    if (!NS_IsMainThread())
        MOZ_CRASH();

    AutoSafeJSContext cx;
    JS_SetRuntimeDebugMode(rt, gDesiredDebugMode);

    nsresult rv;
    const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
    nsCOMPtr<jsdIDebuggerService> jsds = do_GetService(jsdServiceCtrID, &rv);
    if (NS_FAILED(rv)) {
        goto fail;
    }

    if (!JS_SetDebugModeForAllCompartments(cx, gDesiredDebugMode))
        goto fail;

    if (gDesiredDebugMode) {
        rv = jsds->ActivateDebugger(rt);
    }

    gDebugMode = gDesiredDebugMode;
    return;

fail:
    if (jsds)
        jsds->DeactivateDebugger();

    /*
     * If an attempt to turn debug mode on fails, cancel the request. It's
     * always safe to turn debug mode off, since DeactivateDebugger prevents
     * debugger callbacks from having any effect.
     */
    if (gDesiredDebugMode)
        JS_SetRuntimeDebugMode(rt, false);
    gDesiredDebugMode = gDebugMode = false;
}
#else //MOZ_JSDEBUGGER not defined
void
nsXPConnect::CheckForDebugMode(JSRuntime *rt)
{
    gDesiredDebugMode = gDebugMode = false;
}
#endif //#ifdef MOZ_JSDEBUGGER


NS_EXPORT_(void)
xpc_ActivateDebugMode()
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    nsXPConnect::XPConnect()->SetDebugModeWhenPossible(true, true);
    nsXPConnect::CheckForDebugMode(rt->Runtime());
}

/* virtual */
JSContext*
nsXPConnect::GetCurrentJSContext()
{
    return GetRuntime()->GetJSContextStack()->Peek();
}

/* virtual */
JSContext*
nsXPConnect::GetSafeJSContext()
{
    return GetRuntime()->GetJSContextStack()->GetSafeJSContext();
}

namespace xpc {

bool
PushJSContextNoScriptContext(JSContext *aCx)
{
    MOZ_ASSERT_IF(aCx, !GetScriptContextFromJSContext(aCx));
    return XPCJSRuntime::Get()->GetJSContextStack()->Push(aCx);
}

void
PopJSContextNoScriptContext()
{
    XPCJSRuntime::Get()->GetJSContextStack()->Pop();
}

} // namespace xpc

nsIPrincipal*
nsXPConnect::GetPrincipal(JSObject* obj, bool allowShortCircuit) const
{
    MOZ_ASSERT(IS_WN_REFLECTOR(obj), "What kind of wrapper is this?");

    XPCWrappedNative *xpcWrapper = XPCWrappedNative::Get(obj);
    if (xpcWrapper) {
        if (allowShortCircuit) {
            nsIPrincipal *result = xpcWrapper->GetObjectPrincipal();
            if (result) {
                return result;
            }
        }

        // If not, check if it points to an nsIScriptObjectPrincipal
        nsCOMPtr<nsIScriptObjectPrincipal> objPrin =
            do_QueryInterface(xpcWrapper->Native());
        if (objPrin) {
            nsIPrincipal *result = objPrin->GetPrincipal();
            if (result) {
                return result;
            }
        }
    }

    return nullptr;
}

NS_IMETHODIMP
nsXPConnect::HoldObject(JSContext *aJSContext, JSObject *aObjectArg,
                        nsIXPConnectJSObjectHolder **aHolder)
{
    RootedObject aObject(aJSContext, aObjectArg);
    XPCJSObjectHolder* objHolder = XPCJSObjectHolder::newHolder(aObject);
    if (!objHolder)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aHolder = objHolder);
    return NS_OK;
}

namespace xpc {

NS_EXPORT_(bool)
Base64Encode(JSContext *cx, JS::Value val, JS::Value *out)
{
    MOZ_ASSERT(cx);
    MOZ_ASSERT(out);

    JS::RootedValue root(cx, val);
    xpc_qsACString encodedString(cx, root, root.address(), xpc_qsACString::eNull,
                                 xpc_qsACString::eStringify);
    if (!encodedString.IsValid())
        return false;

    nsAutoCString result;
    if (NS_FAILED(mozilla::Base64Encode(encodedString, result))) {
        JS_ReportError(cx, "Failed to encode base64 data!");
        return false;
    }

    JSString *str = JS_NewStringCopyN(cx, result.get(), result.Length());
    if (!str)
        return false;

    *out = STRING_TO_JSVAL(str);
    return true;
}

NS_EXPORT_(bool)
Base64Decode(JSContext *cx, JS::Value val, JS::Value *out)
{
    MOZ_ASSERT(cx);
    MOZ_ASSERT(out);

    JS::RootedValue root(cx, val);
    xpc_qsACString encodedString(cx, root, root.address(), xpc_qsACString::eNull,
                                 xpc_qsACString::eNull);
    if (!encodedString.IsValid())
        return false;

    nsAutoCString result;
    if (NS_FAILED(mozilla::Base64Decode(encodedString, result))) {
        JS_ReportError(cx, "Failed to decode base64 string!");
        return false;
    }

    JSString *str = JS_NewStringCopyN(cx, result.get(), result.Length());
    if (!str)
        return false;

    *out = STRING_TO_JSVAL(str);
    return true;
}

void
SetLocationForGlobal(JSObject *global, const nsACString& location)
{
    MOZ_ASSERT(global);
    EnsureCompartmentPrivate(global)->SetLocation(location);
}

void
SetLocationForGlobal(JSObject *global, nsIURI *locationURI)
{
    MOZ_ASSERT(global);
    EnsureCompartmentPrivate(global)->SetLocationURI(locationURI);
}

} // namespace xpc

NS_IMETHODIMP
nsXPConnect::SetDebugModeWhenPossible(bool mode, bool allowSyncDisable)
{
    gDesiredDebugMode = mode;
    if (!mode && allowSyncDisable)
        CheckForDebugMode(mRuntime->Runtime());
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::GetTelemetryValue(JSContext *cx, jsval *rval)
{
    RootedObject obj(cx, JS_NewObject(cx, nullptr, nullptr, nullptr));
    if (!obj)
        return NS_ERROR_OUT_OF_MEMORY;

    unsigned attrs = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;

    size_t i = JS_SetProtoCalled(cx);
    RootedValue v(cx, DOUBLE_TO_JSVAL(i));
    if (!JS_DefineProperty(cx, obj, "setProto", v, nullptr, nullptr, attrs))
        return NS_ERROR_OUT_OF_MEMORY;

    i = JS_GetCustomIteratorCount(cx);
    v = DOUBLE_TO_JSVAL(i);
    if (!JS_DefineProperty(cx, obj, "customIter", v, nullptr, nullptr, attrs))
        return NS_ERROR_OUT_OF_MEMORY;

    *rval = OBJECT_TO_JSVAL(obj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::NotifyDidPaint()
{
    JS::NotifyDidPaint(GetRuntime()->Runtime());
    return NS_OK;
}

static const uint8_t HAS_PRINCIPALS_FLAG               = 1;
static const uint8_t HAS_ORIGIN_PRINCIPALS_FLAG        = 2;

static nsresult
WriteScriptOrFunction(nsIObjectOutputStream *stream, JSContext *cx,
                      JSScript *scriptArg, HandleObject functionObj)
{
    // Exactly one of script or functionObj must be given
    MOZ_ASSERT(!scriptArg != !functionObj);

    RootedScript script(cx, scriptArg ? scriptArg :
                                        JS_GetFunctionScript(cx, JS_GetObjectFunction(functionObj)));

    nsIPrincipal *principal =
        nsJSPrincipals::get(JS_GetScriptPrincipals(script));
    nsIPrincipal *originPrincipal =
        nsJSPrincipals::get(JS_GetScriptOriginPrincipals(script));

    uint8_t flags = 0;
    if (principal)
        flags |= HAS_PRINCIPALS_FLAG;

    // Optimize for the common case when originPrincipals == principals. As
    // originPrincipals is set to principals when the former is null we can
    // simply skip the originPrincipals when they are the same as principals.
    if (originPrincipal && originPrincipal != principal)
        flags |= HAS_ORIGIN_PRINCIPALS_FLAG;

    nsresult rv = stream->Write8(flags);
    if (NS_FAILED(rv))
        return rv;

    if (flags & HAS_PRINCIPALS_FLAG) {
        rv = stream->WriteObject(principal, true);
        if (NS_FAILED(rv))
            return rv;
    }

    if (flags & HAS_ORIGIN_PRINCIPALS_FLAG) {
        rv = stream->WriteObject(originPrincipal, true);
        if (NS_FAILED(rv))
            return rv;
    }

    uint32_t size;
    void* data;
    {
        if (functionObj)
            data = JS_EncodeInterpretedFunction(cx, functionObj, &size);
        else
            data = JS_EncodeScript(cx, script, &size);
    }

    if (!data)
        return NS_ERROR_OUT_OF_MEMORY;
    MOZ_ASSERT(size);
    rv = stream->Write32(size);
    if (NS_SUCCEEDED(rv))
        rv = stream->WriteBytes(static_cast<char *>(data), size);
    js_free(data);

    return rv;
}

static nsresult
ReadScriptOrFunction(nsIObjectInputStream *stream, JSContext *cx,
                     JSScript **scriptp, JSObject **functionObjp)
{
    // Exactly one of script or functionObj must be given
    MOZ_ASSERT(!scriptp != !functionObjp);

    uint8_t flags;
    nsresult rv = stream->Read8(&flags);
    if (NS_FAILED(rv))
        return rv;

    nsJSPrincipals* principal = nullptr;
    nsCOMPtr<nsIPrincipal> readPrincipal;
    if (flags & HAS_PRINCIPALS_FLAG) {
        rv = stream->ReadObject(true, getter_AddRefs(readPrincipal));
        if (NS_FAILED(rv))
            return rv;
        principal = nsJSPrincipals::get(readPrincipal);
    }

    nsJSPrincipals* originPrincipal = nullptr;
    nsCOMPtr<nsIPrincipal> readOriginPrincipal;
    if (flags & HAS_ORIGIN_PRINCIPALS_FLAG) {
        rv = stream->ReadObject(true, getter_AddRefs(readOriginPrincipal));
        if (NS_FAILED(rv))
            return rv;
        originPrincipal = nsJSPrincipals::get(readOriginPrincipal);
    }

    uint32_t size;
    rv = stream->Read32(&size);
    if (NS_FAILED(rv))
        return rv;

    char* data;
    rv = stream->ReadBytes(size, &data);
    if (NS_FAILED(rv))
        return rv;

    {
        if (scriptp) {
            JSScript *script = JS_DecodeScript(cx, data, size, principal, originPrincipal);
            if (!script)
                rv = NS_ERROR_OUT_OF_MEMORY;
            else
                *scriptp = script;
        } else {
            JSObject *funobj = JS_DecodeInterpretedFunction(cx, data, size,
                                                            principal, originPrincipal);
            if (!funobj)
                rv = NS_ERROR_OUT_OF_MEMORY;
            else
                *functionObjp = funobj;
        }
    }

    nsMemory::Free(data);
    return rv;
}

NS_IMETHODIMP
nsXPConnect::WriteScript(nsIObjectOutputStream *stream, JSContext *cx, JSScript *script)
{
    return WriteScriptOrFunction(stream, cx, script, NullPtr());
}

NS_IMETHODIMP
nsXPConnect::ReadScript(nsIObjectInputStream *stream, JSContext *cx, JSScript **scriptp)
{
    return ReadScriptOrFunction(stream, cx, scriptp, nullptr);
}

NS_IMETHODIMP
nsXPConnect::WriteFunction(nsIObjectOutputStream *stream, JSContext *cx, JSObject *functionObjArg)
{
    RootedObject functionObj(cx, functionObjArg);
    return WriteScriptOrFunction(stream, cx, nullptr, functionObj);
}

NS_IMETHODIMP
nsXPConnect::ReadFunction(nsIObjectInputStream *stream, JSContext *cx, JSObject **functionObjp)
{
    return ReadScriptOrFunction(stream, cx, nullptr, functionObjp);
}

NS_IMETHODIMP
nsXPConnect::MarkErrorUnreported(JSContext *cx)
{
    XPCContext *xpcc = XPCContext::GetXPCContext(cx);
    xpcc->MarkErrorUnreported();
    return NS_OK;
}

/* These are here to be callable from a debugger */
extern "C" {
JS_EXPORT_API(void) DumpJSStack()
{
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if (NS_SUCCEEDED(rv) && xpc)
        xpc->DebugDumpJSStack(true, true, false);
    else
        printf("failed to get XPConnect service!\n");
}

JS_EXPORT_API(char*) PrintJSStack()
{
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    return (NS_SUCCEEDED(rv) && xpc) ?
        xpc->DebugPrintJSStack(true, true, false) :
        nullptr;
}

JS_EXPORT_API(void) DumpJSEval(uint32_t frameno, const char* text)
{
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if (NS_SUCCEEDED(rv) && xpc)
        xpc->DebugDumpEvalInJSStackFrame(frameno, text);
    else
        printf("failed to get XPConnect service!\n");
}

JS_EXPORT_API(void) DumpCompleteHeap()
{
    nsCOMPtr<nsICycleCollectorListener> listener =
      do_CreateInstance("@mozilla.org/cycle-collector-logger;1");
    if (!listener) {
      NS_WARNING("Failed to create CC logger");
      return;
    }

    nsCOMPtr<nsICycleCollectorListener> alltracesListener;
    listener->AllTraces(getter_AddRefs(alltracesListener));
    if (!alltracesListener) {
      NS_WARNING("Failed to get all traces logger");
      return;
    }

    nsJSContext::CycleCollectNow(alltracesListener);
}

} // extern "C"

namespace xpc {

bool
Atob(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!argc)
        return true;

    return xpc::Base64Decode(cx, JS_ARGV(cx, vp)[0], &JS_RVAL(cx, vp));
}

bool
Btoa(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!argc)
        return true;

    return xpc::Base64Encode(cx, JS_ARGV(cx, vp)[0], &JS_RVAL(cx, vp));
}

} // namespace xpc

namespace mozilla {
namespace dom {

bool
IsChromeOrXBL(JSContext* cx, JSObject* /* unused */)
{
    MOZ_ASSERT(NS_IsMainThread());
    JSCompartment* c = js::GetContextCompartment(cx);

    // For remote XUL, we run XBL in the XUL scope. Given that we care about
    // compat and not security for remote XUL, we just always claim to be XBL.
    //
    // Note that, for performance, we don't check AllowXULXBLForPrincipal here,
    // and instead rely on the fact that AllowXBLScope() only returns false in
    // remote XUL situations.
    return AccessCheck::isChrome(c) || IsXBLScope(c) || !AllowXBLScope(c);
}

} // namespace dom
} // namespace mozilla
