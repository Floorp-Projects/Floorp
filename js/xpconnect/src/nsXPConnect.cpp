/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* High level class and public functions implementation. */

#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/Likely.h"
#include "mozilla/Unused.h"

#include "XPCWrapper.h"
#include "jsfriendapi.h"
#include "js/ProfilingStack.h"
#include "GeckoProfiler.h"
#include "nsJSEnvironment.h"
#include "nsThreadUtils.h"
#include "nsDOMJSUtils.h"

#include "WrapperFactory.h"
#include "AccessCheck.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/Promise.h"

#include "nsDOMMutationObserver.h"
#include "nsICycleCollectorListener.h"
#include "nsCycleCollector.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"
#include "nsContentUtils.h"
#include "nsScriptError.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace xpc;
using namespace JS;

NS_IMPL_ISUPPORTS(nsXPConnect, nsIXPConnect)

nsXPConnect* nsXPConnect::gSelf = nullptr;
bool nsXPConnect::gOnceAliveNowDead = false;

// Global cache of the default script security manager (QI'd to
// nsIScriptSecurityManager) and the system principal.
nsIScriptSecurityManager* nsXPConnect::gScriptSecurityManager = nullptr;
nsIPrincipal* nsXPConnect::gSystemPrincipal = nullptr;

const char XPC_EXCEPTION_CONTRACTID[] = "@mozilla.org/js/xpc/Exception;1";
const char XPC_CONSOLE_CONTRACTID[] = "@mozilla.org/consoleservice;1";
const char XPC_SCRIPT_ERROR_CONTRACTID[] = "@mozilla.org/scripterror;1";

/***************************************************************************/

// This global should be used very sparingly: only to create and destroy
// nsXPConnect and when creating a new cooperative (non-primary) XPCJSContext.
static XPCJSContext* gPrimaryContext;

nsXPConnect::nsXPConnect() : mShuttingDown(false) {
  XPCJSContext::InitTLS();

#ifdef MOZ_GECKO_PROFILER
  JS::SetProfilingThreadCallbacks(profiler_register_thread,
                                  profiler_unregister_thread);
#endif

  XPCJSContext* xpccx = XPCJSContext::NewXPCJSContext(nullptr);
  if (!xpccx) {
    MOZ_CRASH("Couldn't create XPCJSContext.");
  }
  gPrimaryContext = xpccx;
  mRuntime = xpccx->Runtime();
}

nsXPConnect::~nsXPConnect() {
  MOZ_ASSERT(XPCJSContext::Get() == gPrimaryContext);

  mRuntime->DeleteSingletonScopes();

  // In order to clean up everything properly, we need to GC twice: once now,
  // to clean anything that can go away on its own (like the Junk Scope, which
  // we unrooted above), and once after forcing a bunch of shutdown in
  // XPConnect, to clean the stuff we forcibly disconnected. The forced
  // shutdown code defaults to leaking in a number of situations, so we can't
  // get by with only the second GC. :-(
  mRuntime->GarbageCollect(JS::GCReason::XPCONNECT_SHUTDOWN);

  mShuttingDown = true;
  XPCWrappedNativeScope::SystemIsBeingShutDown();
  mRuntime->SystemIsBeingShutDown();

  // The above causes us to clean up a bunch of XPConnect data structures,
  // after which point we need to GC to clean everything up. We need to do
  // this before deleting the XPCJSContext, because doing so destroys the
  // maps that our finalize callback depends on.
  mRuntime->GarbageCollect(JS::GCReason::XPCONNECT_SHUTDOWN);

  NS_RELEASE(gSystemPrincipal);
  gScriptSecurityManager = nullptr;

  // shutdown the logging system
  XPC_LOG_FINISH();

  delete gPrimaryContext;

  MOZ_ASSERT(gSelf == this);
  gSelf = nullptr;
  gOnceAliveNowDead = true;
}

// static
void nsXPConnect::InitStatics() {
#ifdef NS_BUILD_REFCNT_LOGGING
  // These functions are used for reporting leaks, so we register them as early
  // as possible to avoid missing any classes' creations.
  js::SetLogCtorDtorFunctions(NS_LogCtor, NS_LogDtor);
#endif

  gSelf = new nsXPConnect();
  gOnceAliveNowDead = false;

  // Initial extra ref to keep the singleton alive
  // balanced by explicit call to ReleaseXPConnectSingleton()
  NS_ADDREF(gSelf);

  // Fire up the SSM.
  nsScriptSecurityManager::InitStatics();
  gScriptSecurityManager = nsScriptSecurityManager::GetScriptSecurityManager();
  gScriptSecurityManager->GetSystemPrincipal(&gSystemPrincipal);
  MOZ_RELEASE_ASSERT(gSystemPrincipal);

  JSContext* cx = XPCJSContext::Get()->Context();
  if (!JS::InitSelfHostedCode(cx)) {
    MOZ_CRASH("InitSelfHostedCode failed");
  }
  if (!gSelf->mRuntime->InitializeStrings(cx)) {
    MOZ_CRASH("InitializeStrings failed");
  }

  // Initialize our singleton scopes.
  gSelf->mRuntime->InitSingletonScopes();

  mozJSComponentLoader::InitStatics();
}

// static
void nsXPConnect::ReleaseXPConnectSingleton() {
  nsXPConnect* xpc = gSelf;
  if (xpc) {
    nsrefcnt cnt;
    NS_RELEASE2(xpc, cnt);
  }

  mozJSComponentLoader::Shutdown();
}

// static
XPCJSRuntime* nsXPConnect::GetRuntimeInstance() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  return gSelf->mRuntime;
}

// static
bool nsXPConnect::IsISupportsDescendant(const nsXPTInterfaceInfo* info) {
  return info && info->HasAncestor(NS_GET_IID(nsISupports));
}

void xpc::ErrorBase::Init(JSErrorBase* aReport) {
  if (!aReport->filename) {
    mFileName.SetIsVoid(true);
  } else {
    CopyUTF8toUTF16(mozilla::MakeStringSpan(aReport->filename), mFileName);
  }

  mSourceId = aReport->sourceId;
  mLineNumber = aReport->lineno;
  mColumn = aReport->column;
}

void xpc::ErrorNote::Init(JSErrorNotes::Note* aNote) {
  xpc::ErrorBase::Init(aNote);

  ErrorNoteToMessageString(aNote, mErrorMsg);
}

void xpc::ErrorReport::Init(JSErrorReport* aReport, const char* aToStringResult,
                            bool aIsChrome, uint64_t aWindowID) {
  xpc::ErrorBase::Init(aReport);
  mCategory = aIsChrome ? NS_LITERAL_CSTRING("chrome javascript")
                        : NS_LITERAL_CSTRING("content javascript");
  mWindowID = aWindowID;

  ErrorReportToMessageString(aReport, mErrorMsg);
  if (mErrorMsg.IsEmpty() && aToStringResult) {
    AppendUTF8toUTF16(mozilla::MakeStringSpan(aToStringResult), mErrorMsg);
  }

  mSourceLine.Assign(aReport->linebuf(), aReport->linebufLength());
  const JSErrorFormatString* efs =
      js::GetErrorMessage(nullptr, aReport->errorNumber);

  if (efs == nullptr) {
    mErrorMsgName.AssignLiteral("");
  } else {
    mErrorMsgName.AssignASCII(efs->name);
  }

  mFlags = aReport->flags;
  mIsMuted = aReport->isMuted;

  if (aReport->notes) {
    if (!mNotes.SetLength(aReport->notes->length(), fallible)) {
      return;
    }

    size_t i = 0;
    for (auto&& note : *aReport->notes) {
      mNotes.ElementAt(i).Init(note.get());
      i++;
    }
  }
}

void xpc::ErrorReport::Init(JSContext* aCx, mozilla::dom::Exception* aException,
                            bool aIsChrome, uint64_t aWindowID) {
  mCategory = aIsChrome ? NS_LITERAL_CSTRING("chrome javascript")
                        : NS_LITERAL_CSTRING("content javascript");
  mWindowID = aWindowID;

  aException->GetErrorMessage(mErrorMsg);

  aException->GetFilename(aCx, mFileName);
  if (mFileName.IsEmpty()) {
    mFileName.SetIsVoid(true);
  }
  mSourceId = aException->SourceId(aCx);
  mLineNumber = aException->LineNumber(aCx);
  mColumn = aException->ColumnNumber();

  mFlags = JSREPORT_EXCEPTION;
}

static LazyLogModule gJSDiagnostics("JSDiagnostics");

void xpc::ErrorBase::AppendErrorDetailsTo(nsCString& error) {
  AppendUTF16toUTF8(mFileName, error);
  error.AppendLiteral(", line ");
  error.AppendInt(mLineNumber, 10);
  error.AppendLiteral(": ");
  AppendUTF16toUTF8(mErrorMsg, error);
}

void xpc::ErrorNote::LogToStderr() {
  if (!nsJSUtils::DumpEnabled()) {
    return;
  }

  nsAutoCString error;
  error.AssignLiteral("JavaScript note: ");
  AppendErrorDetailsTo(error);

  fprintf(stderr, "%s\n", error.get());
  fflush(stderr);
}

void xpc::ErrorReport::LogToStderr() {
  if (!nsJSUtils::DumpEnabled()) {
    return;
  }

  nsAutoCString error;
  error.AssignLiteral("JavaScript ");
  if (JSREPORT_IS_STRICT(mFlags)) {
    error.AppendLiteral("strict ");
  }
  if (JSREPORT_IS_WARNING(mFlags)) {
    error.AppendLiteral("warning: ");
  } else {
    error.AppendLiteral("error: ");
  }
  AppendErrorDetailsTo(error);

  fprintf(stderr, "%s\n", error.get());
  fflush(stderr);

  for (size_t i = 0, len = mNotes.Length(); i < len; i++) {
    ErrorNote& note = mNotes[i];
    note.LogToStderr();
  }
}

void xpc::ErrorReport::LogToConsole() {
  LogToConsoleWithStack(nullptr, nullptr);
}

void xpc::ErrorReport::LogToConsoleWithStack(
    JS::HandleObject aStack, JS::HandleObject aStackGlobal,
    uint64_t aTimeWarpTarget /* = 0 */) {
  // Don't log failures after diverging from a recording during replay, as
  // this will cause the associated debugger operation to fail.
  if (recordreplay::HasDivergedFromRecording()) {
    return;
  }

  if (aStack) {
    MOZ_ASSERT(aStackGlobal);
    MOZ_ASSERT(JS_IsGlobalObject(aStackGlobal));
    js::AssertSameCompartment(aStack, aStackGlobal);
  } else {
    MOZ_ASSERT(!aStackGlobal);
  }

  LogToStderr();

  MOZ_LOG(gJSDiagnostics,
          JSREPORT_IS_WARNING(mFlags) ? LogLevel::Warning : LogLevel::Error,
          ("file %s, line %u\n%s", NS_ConvertUTF16toUTF8(mFileName).get(),
           mLineNumber, NS_ConvertUTF16toUTF8(mErrorMsg).get()));

  // Log to the console. We do this last so that we can simply return if
  // there's no console service without affecting the other reporting
  // mechanisms.
  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(consoleService);

  RefPtr<nsScriptErrorBase> errorObject;
  if (mWindowID && aStack) {
    // Only set stack on messages related to a document
    // As we cache messages in the console service,
    // we have to ensure not leaking them after the related
    // context is destroyed and we only track document lifecycle for now.
    errorObject = new nsScriptErrorWithStack(aStack, aStackGlobal);
  } else {
    errorObject = new nsScriptError();
  }
  errorObject->SetErrorMessageName(mErrorMsgName);
  errorObject->SetTimeWarpTarget(aTimeWarpTarget);

  nsresult rv = errorObject->InitWithWindowID(
      mErrorMsg, mFileName, mSourceLine, mLineNumber, mColumn, mFlags,
      mCategory, mWindowID,
      mCategory.Equals(NS_LITERAL_CSTRING("chrome javascript")));
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = errorObject->InitSourceId(mSourceId);
  NS_ENSURE_SUCCESS_VOID(rv);

  for (size_t i = 0, len = mNotes.Length(); i < len; i++) {
    ErrorNote& note = mNotes[i];

    nsScriptErrorNote* noteObject = new nsScriptErrorNote();
    noteObject->Init(note.mErrorMsg, note.mFileName, note.mSourceId,
                     note.mLineNumber, note.mColumn);
    errorObject->AddNote(noteObject);
  }

  consoleService->LogMessage(errorObject);
}

/* static */
void xpc::ErrorNote::ErrorNoteToMessageString(JSErrorNotes::Note* aNote,
                                              nsAString& aString) {
  aString.Truncate();
  if (aNote->message()) {
    aString.Append(NS_ConvertUTF8toUTF16(aNote->message().c_str()));
  }
}

/* static */
void xpc::ErrorReport::ErrorReportToMessageString(JSErrorReport* aReport,
                                                  nsAString& aString) {
  aString.Truncate();
  if (aReport->message()) {
    JSFlatString* name = js::GetErrorTypeName(
        CycleCollectedJSContext::Get()->Context(), aReport->exnType);
    if (name) {
      AssignJSFlatString(aString, name);
      aString.AppendLiteral(": ");
    }
    aString.Append(NS_ConvertUTF8toUTF16(aReport->message().c_str()));
  }
}

/***************************************************************************/

void xpc_TryUnmarkWrappedGrayObject(nsISupports* aWrappedJS) {
  // QIing to nsIXPConnectWrappedJSUnmarkGray may have side effects!
  nsCOMPtr<nsIXPConnectWrappedJSUnmarkGray> wjsug =
      do_QueryInterface(aWrappedJS);
  Unused << wjsug;
  MOZ_ASSERT(!wjsug,
             "One should never be able to QI to "
             "nsIXPConnectWrappedJSUnmarkGray successfully!");
}

/***************************************************************************/
/***************************************************************************/
// nsIXPConnect interface methods...

template <typename T>
static inline T UnexpectedFailure(T rv) {
  NS_ERROR("This is not supposed to fail!");
  return rv;
}

void xpc::TraceXPCGlobal(JSTracer* trc, JSObject* obj) {
  if (js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL) {
    mozilla::dom::TraceProtoAndIfaceCache(trc, obj);
  }

  // We might be called from a GC during the creation of a global, before we've
  // been able to set up the compartment private.
  if (xpc::CompartmentPrivate* priv = xpc::CompartmentPrivate::Get(obj)) {
    MOZ_ASSERT(priv->GetScope());
    priv->GetScope()->TraceInside(trc);
  }
}

namespace xpc {

JSObject* CreateGlobalObject(JSContext* cx, const JSClass* clasp,
                             nsIPrincipal* principal,
                             JS::RealmOptions& aOptions) {
  MOZ_ASSERT(NS_IsMainThread(), "using a principal off the main thread?");
  MOZ_ASSERT(principal);

  MOZ_RELEASE_ASSERT(
      principal != nsContentUtils::GetNullSubjectPrincipal(),
      "The null subject principal is getting inherited - fix that!");

  SiteIdentifier site;
  nsresult rv = BasePrincipal::Cast(principal)->GetSiteIdentifier(site);
  NS_ENSURE_SUCCESS(rv, nullptr);

  RootedObject global(
      cx, JS_NewGlobalObject(cx, clasp, nsJSPrincipals::get(principal),
                             JS::DontFireOnNewGlobalHook, aOptions));
  if (!global) {
    return nullptr;
  }
  JSAutoRealm ar(cx, global);

  RealmPrivate::Init(global, site);

  if (clasp->flags & JSCLASS_DOM_GLOBAL) {
#ifdef DEBUG
    // Verify that the right trace hook is called. Note that this doesn't
    // work right for wrapped globals, since the tracing situation there is
    // more complicated. Manual inspection shows that they do the right
    // thing.  Also note that we only check this for JSCLASS_DOM_GLOBAL
    // classes because xpc::TraceXPCGlobal won't call
    // TraceProtoAndIfaceCache unless that flag is set.
    if (!((const JSClass*)clasp)->isWrappedNative()) {
      VerifyTraceProtoAndIfaceCacheCalledTracer trc(cx);
      TraceChildren(&trc, GCCellPtr(global.get()));
      MOZ_ASSERT(trc.ok,
                 "Trace hook on global needs to call TraceXPCGlobal for "
                 "XPConnect compartments.");
    }
#endif

    const char* className = clasp->name;
    AllocateProtoAndIfaceCache(global, (strcmp(className, "Window") == 0 ||
                                        strcmp(className, "ChromeWindow") == 0)
                                           ? ProtoAndIfaceCache::WindowLike
                                           : ProtoAndIfaceCache::NonWindowLike);
  }

  return global;
}

void InitGlobalObjectOptions(JS::RealmOptions& aOptions,
                             nsIPrincipal* aPrincipal) {
  bool shouldDiscardSystemSource = ShouldDiscardSystemSource();
  bool extraWarningsForSystemJS = ExtraWarningsForSystemJS();

  bool isSystem = nsContentUtils::IsSystemPrincipal(aPrincipal);

  if (isSystem) {
    // Make sure [SecureContext] APIs are visible:
    aOptions.creationOptions().setSecureContext(true);
    aOptions.creationOptions().setClampAndJitterTime(false);
  }

  if (shouldDiscardSystemSource) {
    bool discardSource = isSystem;

    aOptions.behaviors().setDiscardSource(discardSource);
  }

  if (extraWarningsForSystemJS) {
    if (isSystem) {
      aOptions.behaviors().extraWarningsOverride().set(true);
    }
  }
}

bool InitGlobalObject(JSContext* aJSContext, JS::Handle<JSObject*> aGlobal,
                      uint32_t aFlags) {
  // Immediately enter the global's realm so that everything we create
  // ends up there.
  JSAutoRealm ar(aJSContext, aGlobal);

  // Stuff coming through this path always ends up as a DOM global.
  MOZ_ASSERT(js::GetObjectClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL);

  if (!(aFlags & xpc::OMIT_COMPONENTS_OBJECT)) {
    // XPCCallContext gives us an active request needed to save/restore.
    if (!ObjectScope(aGlobal)->AttachComponentsObject(aJSContext) ||
        !XPCNativeWrapper::AttachNewConstructorObject(aJSContext, aGlobal)) {
      return UnexpectedFailure(false);
    }
  }

  if (!(aFlags & xpc::DONT_FIRE_ONNEWGLOBALHOOK)) {
    JS_FireOnNewGlobalObject(aJSContext, aGlobal);
  }

  return true;
}

nsresult InitClassesWithNewWrappedGlobal(JSContext* aJSContext,
                                         nsISupports* aCOMObj,
                                         nsIPrincipal* aPrincipal,
                                         uint32_t aFlags,
                                         JS::RealmOptions& aOptions,
                                         MutableHandleObject aNewGlobal) {
  MOZ_ASSERT(aJSContext, "bad param");
  MOZ_ASSERT(aCOMObj, "bad param");

  // We pass null for the 'extra' pointer during global object creation, so
  // we need to have a principal.
  MOZ_ASSERT(aPrincipal);

  InitGlobalObjectOptions(aOptions, aPrincipal);

  // Call into XPCWrappedNative to make a new global object, scope, and global
  // prototype.
  xpcObjectHelper helper(aCOMObj);
  MOZ_ASSERT(helper.GetScriptableFlags() & XPC_SCRIPTABLE_IS_GLOBAL_OBJECT);
  RefPtr<XPCWrappedNative> wrappedGlobal;
  nsresult rv = XPCWrappedNative::WrapNewGlobal(
      aJSContext, helper, aPrincipal, aFlags & xpc::INIT_JS_STANDARD_CLASSES,
      aOptions, getter_AddRefs(wrappedGlobal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab a copy of the global and enter its compartment.
  RootedObject global(aJSContext, wrappedGlobal->GetFlatJSObject());
  MOZ_ASSERT(JS_IsGlobalObject(global));

  if (!InitGlobalObject(aJSContext, global, aFlags)) {
    return UnexpectedFailure(NS_ERROR_FAILURE);
  }

  {  // Scope for JSAutoRealm
    JSAutoRealm ar(aJSContext, global);
    if (!JS_DefineProfilingFunctions(aJSContext, global)) {
      return UnexpectedFailure(NS_ERROR_OUT_OF_MEMORY);
    }
  }

  aNewGlobal.set(global);
  return NS_OK;
}

}  // namespace xpc

static nsresult NativeInterface2JSObject(JSContext* aCx, HandleObject aScope,
                                         nsISupports* aCOMObj,
                                         nsWrapperCache* aCache,
                                         const nsIID* aIID, bool aAllowWrapping,
                                         MutableHandleValue aVal) {
  JSAutoRealm ar(aCx, aScope);

  nsresult rv;
  xpcObjectHelper helper(aCOMObj, aCache);
  if (!XPCConvert::NativeInterface2JSObject(aCx, aVal, helper, aIID,
                                            aAllowWrapping, &rv)) {
    return rv;
  }

  MOZ_ASSERT(
      aAllowWrapping || !xpc::WrapperFactory::IsXrayWrapper(&aVal.toObject()),
      "Shouldn't be returning a xray wrapper here");

  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::WrapNative(JSContext* aJSContext, JSObject* aScopeArg,
                        nsISupports* aCOMObj, const nsIID& aIID,
                        JSObject** aRetVal) {
  MOZ_ASSERT(aJSContext, "bad param");
  MOZ_ASSERT(aScopeArg, "bad param");
  MOZ_ASSERT(aCOMObj, "bad param");

  RootedObject aScope(aJSContext, aScopeArg);
  RootedValue v(aJSContext);
  nsresult rv = NativeInterface2JSObject(aJSContext, aScope, aCOMObj, nullptr,
                                         &aIID, true, &v);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!v.isObjectOrNull()) {
    return NS_ERROR_FAILURE;
  }

  *aRetVal = v.toObjectOrNull();
  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::WrapNativeToJSVal(JSContext* aJSContext, JSObject* aScopeArg,
                               nsISupports* aCOMObj, nsWrapperCache* aCache,
                               const nsIID* aIID, bool aAllowWrapping,
                               MutableHandleValue aVal) {
  MOZ_ASSERT(aJSContext, "bad param");
  MOZ_ASSERT(aScopeArg, "bad param");
  MOZ_ASSERT(aCOMObj, "bad param");

  RootedObject aScope(aJSContext, aScopeArg);
  return NativeInterface2JSObject(aJSContext, aScope, aCOMObj, aCache, aIID,
                                  aAllowWrapping, aVal);
}

NS_IMETHODIMP
nsXPConnect::WrapJS(JSContext* aJSContext, JSObject* aJSObjArg,
                    const nsIID& aIID, void** result) {
  MOZ_ASSERT(aJSContext, "bad param");
  MOZ_ASSERT(aJSObjArg, "bad param");
  MOZ_ASSERT(result, "bad param");

  *result = nullptr;

  RootedObject aJSObj(aJSContext, aJSObjArg);

  nsresult rv = NS_ERROR_UNEXPECTED;
  if (!XPCConvert::JSObject2NativeInterface(aJSContext, result, aJSObj, &aIID,
                                            nullptr, &rv))
    return rv;
  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::JSValToVariant(JSContext* cx, HandleValue aJSVal,
                            nsIVariant** aResult) {
  MOZ_ASSERT(aResult, "bad param");

  RefPtr<XPCVariant> variant = XPCVariant::newVariant(cx, aJSVal);
  variant.forget(aResult);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::WrapJSAggregatedToNative(nsISupports* aOuter,
                                      JSContext* aJSContext,
                                      JSObject* aJSObjArg, const nsIID& aIID,
                                      void** result) {
  MOZ_ASSERT(aOuter, "bad param");
  MOZ_ASSERT(aJSContext, "bad param");
  MOZ_ASSERT(aJSObjArg, "bad param");
  MOZ_ASSERT(result, "bad param");

  *result = nullptr;

  RootedObject aJSObj(aJSContext, aJSObjArg);
  nsresult rv;
  if (!XPCConvert::JSObject2NativeInterface(aJSContext, result, aJSObj, &aIID,
                                            aOuter, &rv))
    return rv;
  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::GetWrappedNativeOfJSObject(JSContext* aJSContext,
                                        JSObject* aJSObjArg,
                                        nsIXPConnectWrappedNative** _retval) {
  MOZ_ASSERT(aJSContext, "bad param");
  MOZ_ASSERT(aJSObjArg, "bad param");
  MOZ_ASSERT(_retval, "bad param");

  RootedObject aJSObj(aJSContext, aJSObjArg);
  aJSObj = js::CheckedUnwrapDynamic(aJSObj, aJSContext,
                                    /* stopAtWindowProxy = */ false);
  if (!aJSObj || !IS_WN_REFLECTOR(aJSObj)) {
    *_retval = nullptr;
    return NS_ERROR_FAILURE;
  }

  RefPtr<XPCWrappedNative> temp = XPCWrappedNative::Get(aJSObj);
  temp.forget(_retval);
  return NS_OK;
}

static already_AddRefed<nsISupports> ReflectorToISupports(JSObject* reflector) {
  if (!reflector) {
    return nullptr;
  }

  // Try XPCWrappedNatives.
  if (IS_WN_REFLECTOR(reflector)) {
    XPCWrappedNative* wn = XPCWrappedNative::Get(reflector);
    if (!wn) {
      return nullptr;
    }
    nsCOMPtr<nsISupports> native = wn->Native();
    return native.forget();
  }

  // Try DOM objects.  This QI without taking a ref first is safe, because
  // this if non-null our thing will definitely be a DOM object, and we know
  // their QI to nsISupports doesn't do anything weird.
  nsCOMPtr<nsISupports> canonical =
      do_QueryInterface(mozilla::dom::UnwrapDOMObjectToISupports(reflector));
  return canonical.forget();
}

already_AddRefed<nsISupports> xpc::ReflectorToISupportsStatic(
    JSObject* reflector) {
  // Unwrap security wrappers, if allowed.
  return ReflectorToISupports(js::CheckedUnwrapStatic(reflector));
}

already_AddRefed<nsISupports> xpc::ReflectorToISupportsDynamic(
    JSObject* reflector, JSContext* cx) {
  // Unwrap security wrappers, if allowed.
  return ReflectorToISupports(
      js::CheckedUnwrapDynamic(reflector, cx,
                               /* stopAtWindowProxy = */ false));
}

NS_IMETHODIMP
nsXPConnect::CreateSandbox(JSContext* cx, nsIPrincipal* principal,
                           JSObject** _retval) {
  *_retval = nullptr;

  RootedValue rval(cx);
  SandboxOptions options;
  nsresult rv = CreateSandboxObject(cx, &rval, principal, options);
  MOZ_ASSERT(NS_FAILED(rv) || !rval.isPrimitive(),
             "Bad return value from xpc_CreateSandboxObject()!");

  if (NS_SUCCEEDED(rv) && !rval.isPrimitive()) {
    *_retval = rval.toObjectOrNull();
  }

  return rv;
}

NS_IMETHODIMP
nsXPConnect::EvalInSandboxObject(const nsAString& source, const char* filename,
                                 JSContext* cx, JSObject* sandboxArg,
                                 MutableHandleValue rval) {
  if (!sandboxArg) {
    return NS_ERROR_INVALID_ARG;
  }

  RootedObject sandbox(cx, sandboxArg);
  nsCString filenameStr;
  if (filename) {
    filenameStr.Assign(filename);
  } else {
    filenameStr = NS_LITERAL_CSTRING("x-bogus://XPConnect/Sandbox");
  }
  return EvalInSandbox(cx, sandbox, source, filenameStr, 1, rval);
}

NS_IMETHODIMP
nsXPConnect::DebugDump(int16_t depth) {
#ifdef DEBUG
  depth--;
  XPC_LOG_ALWAYS(
      ("nsXPConnect @ %p with mRefCnt = %" PRIuPTR, this, mRefCnt.get()));
  XPC_LOG_INDENT();
  XPC_LOG_ALWAYS(("gSelf @ %p", gSelf));
  XPC_LOG_ALWAYS(("gOnceAliveNowDead is %d", (int)gOnceAliveNowDead));
  XPCWrappedNativeScope::DebugDumpAllScopes(depth);
  XPC_LOG_OUTDENT();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::DebugDumpObject(nsISupports* p, int16_t depth) {
#ifdef DEBUG
  if (!depth) {
    return NS_OK;
  }
  if (!p) {
    XPC_LOG_ALWAYS(("*** Cound not dump object with NULL address"));
    return NS_OK;
  }

  nsCOMPtr<nsIXPConnect> xpc;
  nsCOMPtr<nsIXPConnectWrappedNative> wn;
  nsCOMPtr<nsIXPConnectWrappedJS> wjs;

  if (NS_SUCCEEDED(
          p->QueryInterface(NS_GET_IID(nsIXPConnect), getter_AddRefs(xpc)))) {
    XPC_LOG_ALWAYS(("Dumping a nsIXPConnect..."));
    xpc->DebugDump(depth);
  } else if (NS_SUCCEEDED(p->QueryInterface(
                 NS_GET_IID(nsIXPConnectWrappedNative), getter_AddRefs(wn)))) {
    XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedNative..."));
    wn->DebugDump(depth);
  } else if (NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnectWrappedJS),
                                            getter_AddRefs(wjs)))) {
    XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedJS..."));
    wjs->DebugDump(depth);
  } else {
    XPC_LOG_ALWAYS(("*** Could not dump the nsISupports @ %p", p));
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::DebugDumpJSStack(bool showArgs, bool showLocals,
                              bool showThisProps) {
  xpc_DumpJSStack(showArgs, showLocals, showThisProps);

  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::VariantToJS(JSContext* ctx, JSObject* scopeArg, nsIVariant* value,
                         MutableHandleValue _retval) {
  MOZ_ASSERT(ctx, "bad param");
  MOZ_ASSERT(scopeArg, "bad param");
  MOZ_ASSERT(value, "bad param");

  RootedObject scope(ctx, scopeArg);
  MOZ_ASSERT(js::IsObjectInContextCompartment(scope, ctx));

  nsresult rv = NS_OK;
  if (!XPCVariant::VariantDataToJS(ctx, value, &rv, _retval)) {
    if (NS_FAILED(rv)) {
      return rv;
    }

    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::JSToVariant(JSContext* ctx, HandleValue value,
                         nsIVariant** _retval) {
  MOZ_ASSERT(ctx, "bad param");
  MOZ_ASSERT(_retval, "bad param");

  RefPtr<XPCVariant> variant = XPCVariant::newVariant(ctx, value);
  variant.forget(_retval);
  if (!(*_retval)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

namespace xpc {

bool Base64Encode(JSContext* cx, HandleValue val, MutableHandleValue out) {
  MOZ_ASSERT(cx);

  nsAutoCString encodedString;
  if (!ConvertJSValueToByteString(cx, val, false, encodedString)) {
    return false;
  }

  nsAutoCString result;
  if (NS_FAILED(mozilla::Base64Encode(encodedString, result))) {
    JS_ReportErrorASCII(cx, "Failed to encode base64 data!");
    return false;
  }

  JSString* str = JS_NewStringCopyN(cx, result.get(), result.Length());
  if (!str) {
    return false;
  }

  out.setString(str);
  return true;
}

bool Base64Decode(JSContext* cx, HandleValue val, MutableHandleValue out) {
  MOZ_ASSERT(cx);

  nsAutoCString encodedString;
  if (!ConvertJSValueToByteString(cx, val, false, encodedString)) {
    return false;
  }

  nsAutoCString result;
  if (NS_FAILED(mozilla::Base64Decode(encodedString, result))) {
    JS_ReportErrorASCII(cx, "Failed to decode base64 string!");
    return false;
  }

  JSString* str = JS_NewStringCopyN(cx, result.get(), result.Length());
  if (!str) {
    return false;
  }

  out.setString(str);
  return true;
}

void SetLocationForGlobal(JSObject* global, const nsACString& location) {
  MOZ_ASSERT(global);
  RealmPrivate::Get(global)->SetLocation(location);
}

void SetLocationForGlobal(JSObject* global, nsIURI* locationURI) {
  MOZ_ASSERT(global);
  RealmPrivate::Get(global)->SetLocationURI(locationURI);
}

}  // namespace xpc

static nsresult WriteScriptOrFunction(nsIObjectOutputStream* stream,
                                      JSContext* cx, JSScript* scriptArg,
                                      HandleObject functionObj) {
  // Exactly one of script or functionObj must be given
  MOZ_ASSERT(!scriptArg != !functionObj);

  RootedScript script(cx, scriptArg);
  if (!script) {
    RootedFunction fun(cx, JS_GetObjectFunction(functionObj));
    script.set(JS_GetFunctionScript(cx, fun));
  }

  uint8_t flags = 0;  // We don't have flags anymore.
  nsresult rv = stream->Write8(flags);
  if (NS_FAILED(rv)) {
    return rv;
  }

  TranscodeBuffer buffer;
  TranscodeResult code;
  {
    if (functionObj) {
      code = EncodeInterpretedFunction(cx, buffer, functionObj);
    } else {
      code = EncodeScript(cx, buffer, script);
    }
  }

  if (code != TranscodeResult_Ok) {
    if ((code & TranscodeResult_Failure) != 0) {
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT((code & TranscodeResult_Throw) != 0);
    JS_ClearPendingException(cx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  size_t size = buffer.length();
  if (size > UINT32_MAX) {
    return NS_ERROR_FAILURE;
  }
  rv = stream->Write32(size);
  if (NS_SUCCEEDED(rv)) {
    // Ideally we could just pass "buffer" here.  See bug 1566574.
    rv = stream->WriteBytes(MakeSpan(buffer.begin(), size));
  }

  return rv;
}

static nsresult ReadScriptOrFunction(nsIObjectInputStream* stream,
                                     JSContext* cx, JSScript** scriptp,
                                     JSObject** functionObjp) {
  // Exactly one of script or functionObj must be given
  MOZ_ASSERT(!scriptp != !functionObjp);

  uint8_t flags;
  nsresult rv = stream->Read8(&flags);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We don't serialize mutedError-ness of scripts, which is fine as long as
  // we only serialize system and XUL-y things. We can detect this by checking
  // where the caller wants us to deserialize.
  //
  // CompilationScope() could theoretically GC, so get that out of the way
  // before comparing to the cx global.
  JSObject* loaderGlobal = xpc::CompilationScope();
  MOZ_RELEASE_ASSERT(nsContentUtils::IsSystemCaller(cx) ||
                     CurrentGlobalOrNull(cx) == loaderGlobal);

  uint32_t size;
  rv = stream->Read32(&size);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char* data;
  rv = stream->ReadBytes(size, &data);
  if (NS_FAILED(rv)) {
    return rv;
  }

  TranscodeBuffer buffer;
  buffer.replaceRawBuffer(reinterpret_cast<uint8_t*>(data), size);

  {
    TranscodeResult code;
    if (scriptp) {
      Rooted<JSScript*> script(cx);
      code = DecodeScript(cx, buffer, &script);
      if (code == TranscodeResult_Ok) {
        *scriptp = script.get();
      }
    } else {
      Rooted<JSFunction*> funobj(cx);
      code = DecodeInterpretedFunction(cx, buffer, &funobj);
      if (code == TranscodeResult_Ok) {
        *functionObjp = JS_GetFunctionObject(funobj.get());
      }
    }

    if (code != TranscodeResult_Ok) {
      if ((code & TranscodeResult_Failure) != 0) {
        return NS_ERROR_FAILURE;
      }
      MOZ_ASSERT((code & TranscodeResult_Throw) != 0);
      JS_ClearPendingException(cx);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsXPConnect::WriteScript(nsIObjectOutputStream* stream, JSContext* cx,
                         JSScript* script) {
  return WriteScriptOrFunction(stream, cx, script, nullptr);
}

NS_IMETHODIMP
nsXPConnect::ReadScript(nsIObjectInputStream* stream, JSContext* cx,
                        JSScript** scriptp) {
  return ReadScriptOrFunction(stream, cx, scriptp, nullptr);
}

NS_IMETHODIMP
nsXPConnect::WriteFunction(nsIObjectOutputStream* stream, JSContext* cx,
                           JSObject* functionObjArg) {
  RootedObject functionObj(cx, functionObjArg);
  return WriteScriptOrFunction(stream, cx, nullptr, functionObj);
}

NS_IMETHODIMP
nsXPConnect::ReadFunction(nsIObjectInputStream* stream, JSContext* cx,
                          JSObject** functionObjp) {
  return ReadScriptOrFunction(stream, cx, nullptr, functionObjp);
}

NS_IMETHODIMP
nsXPConnect::GetIsShuttingDown(bool* aIsShuttingDown) {
  if (!aIsShuttingDown) {
    return NS_ERROR_INVALID_ARG;
  }

  *aIsShuttingDown = mShuttingDown;

  return NS_OK;
}

// static
nsIXPConnect* nsIXPConnect::XPConnect() {
  // Do a release-mode assert that we're not doing anything significant in
  // XPConnect off the main thread. If you're an extension developer hitting
  // this, you need to change your code. See bug 716167.
  if (!MOZ_LIKELY(NS_IsMainThread())) {
    MOZ_CRASH();
  }

  return nsXPConnect::gSelf;
}

/* These are here to be callable from a debugger */
extern "C" {

MOZ_EXPORT void DumpJSStack() { xpc_DumpJSStack(true, true, false); }

MOZ_EXPORT void DumpCompleteHeap() {
  nsCOMPtr<nsICycleCollectorListener> listener =
      nsCycleCollector_createLogger();
  MOZ_ASSERT(listener);

  nsCOMPtr<nsICycleCollectorListener> alltracesListener;
  listener->AllTraces(getter_AddRefs(alltracesListener));
  if (!alltracesListener) {
    NS_WARNING("Failed to get all traces logger");
    return;
  }

  nsJSContext::CycleCollectNow(alltracesListener);
}

}  // extern "C"

namespace xpc {

bool Atob(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.length()) {
    return true;
  }

  return xpc::Base64Decode(cx, args[0], args.rval());
}

bool Btoa(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.length()) {
    return true;
  }

  return xpc::Base64Encode(cx, args[0], args.rval());
}

bool IsXrayWrapper(JSObject* obj) { return WrapperFactory::IsXrayWrapper(obj); }

}  // namespace xpc

namespace mozilla {
namespace dom {

bool IsChromeOrXBL(JSContext* cx, JSObject* /* unused */) {
  MOZ_ASSERT(NS_IsMainThread());
  JS::Realm* realm = JS::GetCurrentRealmOrNull(cx);
  MOZ_ASSERT(realm);
  JS::Compartment* c = JS::GetCompartmentForRealm(realm);

  // For remote XUL, we run XBL in the XUL scope. Given that we care about
  // compat and not security for remote XUL, we just always claim to be XBL.
  //
  // Note that, for performance, we don't check AllowXULXBLForPrincipal here,
  // and instead rely on the fact that AllowContentXBLScope() only returns false
  // in remote XUL situations.
  return AccessCheck::isChrome(c) || IsContentXBLCompartment(c) ||
         !AllowContentXBLScope(realm);
}

bool IsNotUAWidget(JSContext* cx, JSObject* /* unused */) {
  MOZ_ASSERT(NS_IsMainThread());
  JS::Realm* realm = JS::GetCurrentRealmOrNull(cx);
  MOZ_ASSERT(realm);
  JS::Compartment* c = JS::GetCompartmentForRealm(realm);

  return !IsUAWidgetCompartment(c);
}

bool IsChromeOrXBLOrUAWidget(JSContext* cx, JSObject* /* unused */) {
  if (IsChromeOrXBL(cx, nullptr)) {
    return true;
  }

  MOZ_ASSERT(NS_IsMainThread());
  JS::Realm* realm = JS::GetCurrentRealmOrNull(cx);
  MOZ_ASSERT(realm);
  JS::Compartment* c = JS::GetCompartmentForRealm(realm);

  return IsUAWidgetCompartment(c);
}

extern bool IsCurrentThreadRunningChromeWorker();

bool ThreadSafeIsChromeOrXBLOrUAWidget(JSContext* cx, JSObject* obj) {
  if (NS_IsMainThread()) {
    return IsChromeOrXBLOrUAWidget(cx, obj);
  }
  return IsCurrentThreadRunningChromeWorker();
}

}  // namespace dom
}  // namespace mozilla

void xpc::CreateCooperativeContext() {
  MOZ_ASSERT(gPrimaryContext);
  XPCJSContext::NewXPCJSContext(gPrimaryContext);
}

void xpc::DestroyCooperativeContext() {
  MOZ_ASSERT(XPCJSContext::Get() != gPrimaryContext);
  delete XPCJSContext::Get();
}

void xpc::YieldCooperativeContext() {
  JS_YieldCooperativeContext(XPCJSContext::Get()->Context());
}

void xpc::ResumeCooperativeContext() {
  JS_ResumeCooperativeContext(XPCJSContext::Get()->Context());
}

void xpc::CacheAutomationPref(bool* aMirror) {
  // The obvious thing is to make this pref a static pref. But then it would
  // always be defined and always show up in about:config, and users could flip
  // it, which we don't want. Instead we roll our own callback so that if the
  // pref is undefined (the normal case) then sAutomationPrefIsSet is false and
  // nothing shows up in about:config.
  nsresult rv = mozilla::Preferences::RegisterCallbackAndCall(
      [](const char* aPrefName, void* aData) {
        auto aMirror = static_cast<bool*>(aData);
        *aMirror =
            mozilla::Preferences::GetBool(aPrefName, /* aFallback */ false);
      },
      "security."
      "turn_off_all_security_so_that_viruses_can_take_over_this_computer",
      static_cast<void*>(aMirror));
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}
