/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "xpcprivate.h" // For AutoCxPusher guts
#include "xpcpublic.h"
#include "nsIGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "nsJSUtils.h"
#include "nsDOMJSUtils.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

static MOZ_THREAD_LOCAL(ScriptSettingsStackEntry*) sScriptSettingsTLS;
static bool sScriptSettingsTLSInitialized;

class ScriptSettingsStack {
public:
  static ScriptSettingsStackEntry* Top() {
    return sScriptSettingsTLS.get();
  }

  static void Push(ScriptSettingsStackEntry *aEntry) {
    MOZ_ASSERT(!aEntry->mOlder);
    // Whenever JSAPI use is disabled, the next stack entry pushed must
    // always be a candidate entry point.
    MOZ_ASSERT_IF(!Top() || Top()->NoJSAPI(), aEntry->mIsCandidateEntryPoint);

    aEntry->mOlder = Top();
    sScriptSettingsTLS.set(aEntry);
  }

  static void Pop(ScriptSettingsStackEntry *aEntry) {
    MOZ_ASSERT(aEntry == Top());
    sScriptSettingsTLS.set(aEntry->mOlder);
  }

  static nsIGlobalObject* IncumbentGlobal() {
    ScriptSettingsStackEntry *entry = Top();
    return entry ? entry->mGlobalObject : nullptr;
  }

  static ScriptSettingsStackEntry* EntryPoint() {
    ScriptSettingsStackEntry *entry = Top();
    if (!entry) {
      return nullptr;
    }
    while (entry) {
      if (entry->mIsCandidateEntryPoint)
        return entry;
      entry = entry->mOlder;
    }
    MOZ_CRASH("Non-empty stack should always have an entry point");
  }

  static nsIGlobalObject* EntryGlobal() {
    ScriptSettingsStackEntry *entry = EntryPoint();
    return entry ? entry->mGlobalObject : nullptr;
  }

};

static unsigned long gRunToCompletionListeners = 0;

void
UseEntryScriptProfiling()
{
  MOZ_ASSERT(NS_IsMainThread());
  ++gRunToCompletionListeners;
}

void
UnuseEntryScriptProfiling()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gRunToCompletionListeners > 0);
  --gRunToCompletionListeners;
}

void
InitScriptSettings()
{
  bool success = sScriptSettingsTLS.init();
  if (!success) {
    MOZ_CRASH();
  }

  sScriptSettingsTLS.set(nullptr);
  sScriptSettingsTLSInitialized = true;
}

void
DestroyScriptSettings()
{
  MOZ_ASSERT(sScriptSettingsTLS.get() == nullptr);
}

bool
ScriptSettingsInitialized()
{
  return sScriptSettingsTLSInitialized;
}

ScriptSettingsStackEntry::ScriptSettingsStackEntry(nsIGlobalObject *aGlobal,
                                                   bool aCandidate)
  : mGlobalObject(aGlobal)
  , mIsCandidateEntryPoint(aCandidate)
  , mOlder(nullptr)
{
  MOZ_ASSERT(mGlobalObject);
  MOZ_ASSERT(mGlobalObject->GetGlobalJSObject(),
             "Must have an actual JS global for the duration on the stack");
  MOZ_ASSERT(JS_IsGlobalObject(mGlobalObject->GetGlobalJSObject()),
             "No outer windows allowed");

  ScriptSettingsStack::Push(this);
}

// This constructor is only for use by AutoNoJSAPI.
ScriptSettingsStackEntry::ScriptSettingsStackEntry()
   : mGlobalObject(nullptr)
   , mIsCandidateEntryPoint(true)
   , mOlder(nullptr)
{
  ScriptSettingsStack::Push(this);
}

ScriptSettingsStackEntry::~ScriptSettingsStackEntry()
{
  // We must have an actual JS global for the entire time this is on the stack.
  MOZ_ASSERT_IF(mGlobalObject, mGlobalObject->GetGlobalJSObject());

  ScriptSettingsStack::Pop(this);
}

// If the entry or incumbent global ends up being something that the subject
// principal doesn't subsume, we don't want to use it. This never happens on
// the web, but can happen with asymmetric privilege relationships (i.e.
// nsExpandedPrincipal and System Principal).
//
// The most correct thing to use instead would be the topmost global on the
// callstack whose principal is subsumed by the subject principal. But that's
// hard to compute, so we just substitute the global of the current
// compartment. In practice, this is fine.
//
// Note that in particular things like:
//
// |SpecialPowers.wrap(crossOriginWindow).eval(open())|
//
// trigger this case. Although both the entry global and the current global
// have normal principals, the use of Gecko-specific System-Principaled JS
// puts the code from two different origins on the callstack at once, which
// doesn't happen normally on the web.
static nsIGlobalObject*
ClampToSubject(nsIGlobalObject* aGlobalOrNull)
{
  if (!aGlobalOrNull || !NS_IsMainThread()) {
    return aGlobalOrNull;
  }

  nsIPrincipal* globalPrin = aGlobalOrNull->PrincipalOrNull();
  NS_ENSURE_TRUE(globalPrin, GetCurrentGlobal());
  if (!nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller()->SubsumesConsideringDomain(globalPrin)) {
    return GetCurrentGlobal();
  }

  return aGlobalOrNull;
}

nsIGlobalObject*
GetEntryGlobal()
{
  return ClampToSubject(ScriptSettingsStack::EntryGlobal());
}

nsIDocument*
GetEntryDocument()
{
  nsIGlobalObject* global = GetEntryGlobal();
  nsCOMPtr<nsPIDOMWindowInner> entryWin = do_QueryInterface(global);

  // If our entry global isn't a window, see if it's an addon scope associated
  // with a window. If it is, the caller almost certainly wants that rather
  // than null.
  if (!entryWin && global) {
    if (auto* win = xpc::AddonWindowOrNull(global->GetGlobalJSObject())) {
      entryWin = win->AsInner();
    }
  }

  return entryWin ? entryWin->GetExtantDoc() : nullptr;
}

nsIGlobalObject*
GetIncumbentGlobal()
{
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry global on the stack, and therefore no incumbent
  // global either.
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::EntryGlobal() == nullptr);
    return nullptr;
  }

  // See what the JS engine has to say. If we've got a scripted caller
  // override in place, the JS engine will lie to us and pretend that
  // there's nothing on the JS stack, which will cause us to check the
  // incumbent script stack below.
  if (JSObject *global = JS::GetScriptedCallerGlobal(cx)) {
    return ClampToSubject(xpc::NativeGlobal(global));
  }

  // Ok, nothing from the JS engine. Let's use whatever's on the
  // explicit stack.
  return ClampToSubject(ScriptSettingsStack::IncumbentGlobal());
}

nsIGlobalObject*
GetCurrentGlobal()
{
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    return nullptr;
  }

  JSObject *global = JS::CurrentGlobalOrNull(cx);
  if (!global) {
    return nullptr;
  }

  return xpc::NativeGlobal(global);
}

nsIPrincipal*
GetWebIDLCallerPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  ScriptSettingsStackEntry *entry = ScriptSettingsStack::EntryPoint();

  // If we have an entry point that is not NoJSAPI, we know it must be an
  // AutoEntryScript.
  if (!entry || entry->NoJSAPI()) {
    return nullptr;
  }
  AutoEntryScript* aes = static_cast<AutoEntryScript*>(entry);

  // We can't yet rely on the Script Settings Stack to properly determine the
  // entry script, because there are still lots of places in the tree where we
  // don't yet use an AutoEntryScript (bug 951991 tracks this work). In the
  // mean time though, we can make some observations to hack around the
  // problem:
  //
  // (1) All calls into JS-implemented WebIDL go through CallSetup, which goes
  //     through AutoEntryScript.
  // (2) The top candidate entry point in the Script Settings Stack is the
  //     entry point if and only if no other JSContexts have been pushed on
  //     top of the push made by that entry's AutoEntryScript.
  //
  // Because of (1), all of the cases where we might return a non-null
  // WebIDL Caller are guaranteed to have put an entry on the Script Settings
  // Stack, so we can restrict our search to that. Moreover, (2) gives us a
  // criterion to determine whether an entry in the Script Setting Stack means
  // that we should return a non-null WebIDL Caller.
  //
  // Once we fix bug 951991, this can all be simplified.
  if (!aes->CxPusherIsStackTop()) {
    return nullptr;
  }

  return aes->mWebIDLCallerPrincipal;
}

static JSContext*
FindJSContext(nsIGlobalObject* aGlobalObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  JSContext *cx = nullptr;
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobalObject);
  if (sgo && sgo->GetScriptContext()) {
    cx = sgo->GetScriptContext()->GetNativeContext();
  }
  if (!cx) {
    cx = nsContentUtils::GetSafeJSContext();
  }
  return cx;
}

AutoJSAPI::AutoJSAPI()
  : mCx(nullptr)
  , mOwnErrorReporting(false)
  , mOldAutoJSAPIOwnsErrorReporting(false)
  , mIsMainThread(false) // For lack of anything better
{
}

AutoJSAPI::~AutoJSAPI()
{
  if (mOwnErrorReporting) {
    ReportException();

    // We need to do this _after_ processing the existing exception, because the
    // JS engine can throw while doing that, and uses this bit to determine what
    // to do in that case: squelch the exception if the bit is set, otherwise
    // call the error reporter. Calling WarningOnlyErrorReporter with a
    // non-warning will assert, so we need to make sure we do the former.
    JS::ContextOptionsRef(cx()).setAutoJSAPIOwnsErrorReporting(mOldAutoJSAPIOwnsErrorReporting);
  }

  if (mOldErrorReporter.isSome()) {
    JS_SetErrorReporter(JS_GetRuntime(cx()), mOldErrorReporter.value());
  }
}

void
AutoJSAPI::InitInternal(JSObject* aGlobal, JSContext* aCx, bool aIsMainThread)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aIsMainThread == NS_IsMainThread());
#ifdef DEBUG
  bool haveException = JS_IsExceptionPending(aCx);
#endif // DEBUG

  mCx = aCx;
  mIsMainThread = aIsMainThread;
  if (aIsMainThread) {
    // This Rooted<> is necessary only as long as AutoCxPusher::AutoCxPusher
    // can GC, which is only possible because XPCJSContextStack::Push calls
    // nsIPrincipal.Equals. Once that is removed, the Rooted<> will no longer
    // be necessary.
    JS::Rooted<JSObject*> global(JS_GetRuntime(aCx), aGlobal);
    mCxPusher.emplace(mCx);
    mAutoNullableCompartment.emplace(mCx, global);
  } else {
    mAutoNullableCompartment.emplace(mCx, aGlobal);
  }

  JSRuntime* rt = JS_GetRuntime(aCx);
  mOldErrorReporter.emplace(JS_GetErrorReporter(rt));

  if (aIsMainThread) {
    JS_SetErrorReporter(rt, xpc::SystemErrorReporter);
  }

#ifdef DEBUG
  if (haveException) {
    JS::Rooted<JS::Value> exn(aCx);
    JS_GetPendingException(aCx, &exn);

    JS_ClearPendingException(aCx);
    if (exn.isObject()) {
      JS::Rooted<JSObject*> exnObj(aCx, &exn.toObject());

      nsAutoJSString stack, filename, name, message;
      int32_t line;

      JS::Rooted<JS::Value> tmp(aCx);
      if (!JS_GetProperty(aCx, exnObj, "filename", &tmp)) {
        JS_ClearPendingException(aCx);
      }
      if (tmp.isUndefined()) {
        if (!JS_GetProperty(aCx, exnObj, "fileName", &tmp)) {
          JS_ClearPendingException(aCx);
        }
      }

      if (!filename.init(aCx, tmp)) {
        JS_ClearPendingException(aCx);
      }

      if (!JS_GetProperty(aCx, exnObj, "stack", &tmp) ||
          !stack.init(aCx, tmp)) {
        JS_ClearPendingException(aCx);
      }

      if (!JS_GetProperty(aCx, exnObj, "name", &tmp) ||
          !name.init(aCx, tmp)) {
        JS_ClearPendingException(aCx);
      }

      if (!JS_GetProperty(aCx, exnObj, "message", &tmp) ||
          !message.init(aCx, tmp)) {
        JS_ClearPendingException(aCx);
      }

      if (!JS_GetProperty(aCx, exnObj, "lineNumber", &tmp) ||
          !JS::ToInt32(aCx, tmp, &line)) {
        JS_ClearPendingException(aCx);
        line = 0;
      }

      printf_stderr("PREEXISTING EXCEPTION OBJECT: '%s: %s'\n%s:%d\n%s\n",
                    NS_ConvertUTF16toUTF8(name).get(),
                    NS_ConvertUTF16toUTF8(message).get(),
                    NS_ConvertUTF16toUTF8(filename).get(), line,
                    NS_ConvertUTF16toUTF8(stack).get());
    } else {
      // It's a primitive... not much we can do other than stringify it.
      nsAutoJSString exnStr;
      if (!exnStr.init(aCx, exn)) {
        JS_ClearPendingException(aCx);
      }

      printf_stderr("PREEXISTING EXCEPTION PRIMITIVE: %s\n",
                    NS_ConvertUTF16toUTF8(exnStr).get());
    }
    MOZ_ASSERT(false, "We had an exception; we should not have");
  }
#endif // DEBUG
}

AutoJSAPI::AutoJSAPI(nsIGlobalObject* aGlobalObject,
                     bool aIsMainThread,
                     JSContext* aCx)
  : mOwnErrorReporting(false)
  , mOldAutoJSAPIOwnsErrorReporting(false)
  , mIsMainThread(aIsMainThread)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aGlobalObject->GetGlobalJSObject(), "Must have a JS global");
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aIsMainThread == NS_IsMainThread());

  InitInternal(aGlobalObject->GetGlobalJSObject(), aCx, aIsMainThread);
}

void
AutoJSAPI::Init()
{
  MOZ_ASSERT(!mCx, "An AutoJSAPI should only be initialised once");

  InitInternal(/* aGlobal */ nullptr,
               nsContentUtils::GetDefaultJSContextForThread(),
               NS_IsMainThread());
}

bool
AutoJSAPI::Init(nsIGlobalObject* aGlobalObject, JSContext* aCx)
{
  MOZ_ASSERT(!mCx, "An AutoJSAPI should only be initialised once");
  MOZ_ASSERT(aCx);

  if (NS_WARN_IF(!aGlobalObject)) {
    return false;
  }

  JSObject* global = aGlobalObject->GetGlobalJSObject();
  if (NS_WARN_IF(!global)) {
    return false;
  }

  InitInternal(global, aCx, NS_IsMainThread());
  return true;
}

bool
AutoJSAPI::Init(nsIGlobalObject* aGlobalObject)
{
  return Init(aGlobalObject, nsContentUtils::GetDefaultJSContextForThread());
}

bool
AutoJSAPI::Init(JSObject* aObject)
{
  return Init(xpc::NativeGlobal(aObject));
}

bool
AutoJSAPI::Init(nsPIDOMWindowInner* aWindow, JSContext* aCx)
{
  return Init(nsGlobalWindow::Cast(aWindow), aCx);
}

bool
AutoJSAPI::Init(nsPIDOMWindowInner* aWindow)
{
  return Init(nsGlobalWindow::Cast(aWindow));
}

bool
AutoJSAPI::Init(nsGlobalWindow* aWindow, JSContext* aCx)
{
  return Init(static_cast<nsIGlobalObject*>(aWindow), aCx);
}

bool
AutoJSAPI::Init(nsGlobalWindow* aWindow)
{
  return Init(static_cast<nsIGlobalObject*>(aWindow));
}

// Even with autoJSAPIOwnsErrorReporting, the JS engine still sends warning
// reports to the JSErrorReporter as soon as they are generated. These go
// directly to the console, so we can handle them easily here.
//
// Eventually, SpiderMonkey will have a special-purpose callback for warnings
// only.
void
WarningOnlyErrorReporter(JSContext* aCx, const char* aMessage, JSErrorReport* aRep)
{
  MOZ_ASSERT(JSREPORT_IS_WARNING(aRep->flags));
  if (!NS_IsMainThread()) {
    // Reporting a warning on workers is a bit complicated because we have to
    // climb our parent chain until we get to the main thread.  So go ahead and
    // just go through the worker ReportError codepath here.
    //
    // That said, it feels like we should be able to short-circuit things a bit
    // here by posting an appropriate runnable to the main thread directly...
    // Worth looking into sometime.
    workers::WorkerPrivate* worker = workers::GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(worker);

    worker->ReportError(aCx, aMessage, aRep);
    return;
  }

  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  nsGlobalWindow* win = xpc::CurrentWindowOrNull(aCx);
  if (!win) {
    // We run addons in a separate privileged compartment, but if we're in an
    // addon compartment we should log warnings to the console of the associated
    // DOM Window.
    win = xpc::AddonWindowOrNull(JS::CurrentGlobalOrNull(aCx));
  }
  xpcReport->Init(aRep, aMessage, nsContentUtils::IsCallerChrome(),
                  win ? win->AsInner()->WindowID() : 0);
  xpcReport->LogToConsole();
}

void
AutoJSAPI::TakeOwnershipOfErrorReporting()
{
  MOZ_ASSERT(!mOwnErrorReporting);
  mOwnErrorReporting = true;

  JSRuntime *rt = JS_GetRuntime(cx());
  mOldAutoJSAPIOwnsErrorReporting = JS::ContextOptionsRef(cx()).autoJSAPIOwnsErrorReporting();
  JS::ContextOptionsRef(cx()).setAutoJSAPIOwnsErrorReporting(true);
  JS_SetErrorReporter(rt, WarningOnlyErrorReporter);
}

void
AutoJSAPI::ReportException()
{
  MOZ_ASSERT(OwnsErrorReporting(), "This is not our exception to report!");
  if (!HasException()) {
    return;
  }

  // AutoJSAPI uses a JSAutoNullableCompartment, and may be in a null
  // compartment when the destructor is called. However, the JS engine
  // requires us to be in a compartment when we fetch the pending exception.
  // In this case, we enter the privileged junk scope and don't dispatch any
  // error events.
  JS::Rooted<JSObject*> errorGlobal(cx(), JS::CurrentGlobalOrNull(cx()));
  if (!errorGlobal) {
    if (mIsMainThread) {
      errorGlobal = xpc::PrivilegedJunkScope();
    } else {
      errorGlobal = workers::GetCurrentThreadWorkerGlobal();
    }
  }
  JSAutoCompartment ac(cx(), errorGlobal);
  JS::Rooted<JS::Value> exn(cx());
  js::ErrorReport jsReport(cx());
  if (StealException(&exn) && jsReport.init(cx(), exn)) {
    if (mIsMainThread) {
      RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();

      RefPtr<nsGlobalWindow> win = xpc::WindowGlobalOrNull(errorGlobal);
      if (!win) {
        // We run addons in a separate privileged compartment, but they still
        // expect to trigger the onerror handler of their associated DOM Window.
        win = xpc::AddonWindowOrNull(errorGlobal);
      }
      nsPIDOMWindowInner* inner = win ? win->AsInner() : nullptr;
      xpcReport->Init(jsReport.report(), jsReport.message(),
                      nsContentUtils::IsCallerChrome(),
                      inner ? inner->WindowID() : 0);
      if (inner && jsReport.report()->errorNumber != JSMSG_OUT_OF_MEMORY) {
        DispatchScriptErrorEvent(inner, JS_GetRuntime(cx()), xpcReport, exn);
      } else {
        JS::Rooted<JSObject*> stack(cx(),
          xpc::FindExceptionStackForConsoleReport(inner, exn));
        xpcReport->LogToConsoleWithStack(stack);
      }
    } else {
      // On a worker, we just use the worker error reporting mechanism and don't
      // bother with xpc::ErrorReport.  This will ensure that all the right
      // events (which are a lot more complicated than in the window case) get
      // fired.
      workers::WorkerPrivate* worker = workers::GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(worker);
      MOZ_ASSERT(worker->GetJSContext() == cx());
      // Before invoking ReportError, put the exception back on the context,
      // because it may want to put it in its error events and has no other way
      // to get hold of it.  After we invoke ReportError, clear the exception on
      // cx(), just in case ReportError didn't.
      JS_SetPendingException(cx(), exn);
      worker->ReportError(cx(), jsReport.message(), jsReport.report());
      ClearException();
    }
  } else {
    NS_WARNING("OOMed while acquiring uncaught exception from JSAPI");
    ClearException();
  }
}

bool
AutoJSAPI::PeekException(JS::MutableHandle<JS::Value> aVal)
{
  MOZ_ASSERT_IF(mIsMainThread, CxPusherIsStackTop());
  MOZ_ASSERT(HasException());
  MOZ_ASSERT(js::GetContextCompartment(cx()));
  if (!JS_GetPendingException(cx(), aVal)) {
    return false;
  }
  return true;
}

bool
AutoJSAPI::StealException(JS::MutableHandle<JS::Value> aVal)
{
  if (!PeekException(aVal)) {
    return false;
  }
  JS_ClearPendingException(cx());
  return true;
}

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 const char *aReason,
                                 bool aIsMainThread,
                                 JSContext* aCx)
  : AutoJSAPI(aGlobalObject, aIsMainThread,
              aCx ? aCx : FindJSContext(aGlobalObject))
  , ScriptSettingsStackEntry(aGlobalObject, /* aCandidate = */ true)
  , mWebIDLCallerPrincipal(nullptr)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT_IF(!aCx, aIsMainThread); // cx is mandatory off-main-thread.
  MOZ_ASSERT_IF(aCx && aIsMainThread, aCx == FindJSContext(aGlobalObject));

  if (aIsMainThread && gRunToCompletionListeners > 0) {
    mDocShellEntryMonitor.emplace(cx(), aReason);
  }

  TakeOwnershipOfErrorReporting();
}

AutoEntryScript::AutoEntryScript(JSObject* aObject,
                                 const char *aReason,
                                 bool aIsMainThread,
                                 JSContext* aCx)
  : AutoEntryScript(xpc::NativeGlobal(aObject), aReason, aIsMainThread, aCx)
{
}

AutoEntryScript::~AutoEntryScript()
{
  // GC when we pop a script entry point. This is a useful heuristic that helps
  // us out on certain (flawed) benchmarks like sunspider, because it lets us
  // avoid GCing during the timing loop.
  JS_MaybeGC(cx());
}

AutoEntryScript::DocshellEntryMonitor::DocshellEntryMonitor(JSContext* aCx,
                                                            const char* aReason)
  : JS::dbg::AutoEntryMonitor(aCx)
  , mReason(aReason)
{
}

void
AutoEntryScript::DocshellEntryMonitor::Entry(JSContext* aCx, JSFunction* aFunction,
                                             JSScript* aScript, JS::Handle<JS::Value> aAsyncStack,
                                             JS::Handle<JSString*> aAsyncCause)
{
  JS::Rooted<JSFunction*> rootedFunction(aCx);
  if (aFunction) {
    rootedFunction = aFunction;
  }
  JS::Rooted<JSScript*> rootedScript(aCx);
  if (aScript) {
    rootedScript = aScript;
  }

  nsCOMPtr<nsPIDOMWindowInner> window =
    do_QueryInterface(xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx)));
  if (!window || !window->GetDocShell() ||
      !window->GetDocShell()->GetRecordProfileTimelineMarkers()) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShellForJSRunToCompletion = window->GetDocShell();
  nsString filename;
  uint32_t lineNumber = 0;

  js::AutoStableStringChars functionName(aCx);
  if (rootedFunction) {
    JS::Rooted<JSString*> displayId(aCx, JS_GetFunctionDisplayId(rootedFunction));
    if (displayId) {
      if (!functionName.initTwoByte(aCx, displayId)) {
        JS_ClearPendingException(aCx);
        return;
      }
    }
  }

  if (!rootedScript) {
    rootedScript = JS_GetFunctionScript(aCx, rootedFunction);
  }
  if (rootedScript) {
    filename = NS_ConvertUTF8toUTF16(JS_GetScriptFilename(rootedScript));
    lineNumber = JS_GetScriptBaseLineNumber(aCx, rootedScript);
  }

  if (!filename.IsEmpty() || functionName.isTwoByte()) {
    const char16_t* functionNameChars = functionName.isTwoByte() ?
      functionName.twoByteChars() : nullptr;

    JS::Rooted<JS::Value> asyncCauseValue(aCx, aAsyncCause ? StringValue(aAsyncCause) :
                                          JS::NullValue());
    docShellForJSRunToCompletion->NotifyJSRunToCompletionStart(mReason,
                                                               functionNameChars,
                                                               filename.BeginReading(),
                                                               lineNumber, aAsyncStack,
                                                               asyncCauseValue);
  }
}

void
AutoEntryScript::DocshellEntryMonitor::Exit(JSContext* aCx)
{
  nsCOMPtr<nsPIDOMWindowInner> window =
    do_QueryInterface(xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx)));
  // Not really worth checking GetRecordProfileTimelineMarkers here.
  if (window && window->GetDocShell()) {
    nsCOMPtr<nsIDocShell> docShellForJSRunToCompletion = window->GetDocShell();
    docShellForJSRunToCompletion->NotifyJSRunToCompletionStop();
  }
}

AutoIncumbentScript::AutoIncumbentScript(nsIGlobalObject* aGlobalObject)
  : ScriptSettingsStackEntry(aGlobalObject, /* aCandidate = */ false)
  , mCallerOverride(nsContentUtils::GetCurrentJSContextForThread())
{
}

AutoNoJSAPI::AutoNoJSAPI(bool aIsMainThread)
  : ScriptSettingsStackEntry()
{
  if (aIsMainThread) {
    mCxPusher.emplace(static_cast<JSContext*>(nullptr),
                      /* aAllowNull = */ true);
  }
}

danger::AutoCxPusher::AutoCxPusher(JSContext* cx, bool allowNull)
{
  MOZ_ASSERT_IF(!allowNull, cx);

  // Hold a strong ref to the nsIScriptContext, if any. This ensures that we
  // only destroy the mContext of an nsJSContext when it is not on the cx stack
  // (and therefore not in use). See nsJSContext::DestroyJSContext().
  if (cx)
    mScx = GetScriptContextFromJSContext(cx);

  XPCJSContextStack *stack = XPCJSRuntime::Get()->GetJSContextStack();
  if (!stack->Push(cx)) {
    MOZ_CRASH();
  }
  mStackDepthAfterPush = stack->Count();

#ifdef DEBUG
  mPushedContext = cx;
  mCompartmentDepthOnEntry = cx ? js::GetEnterCompartmentDepth(cx) : 0;
#endif

  // Enter a request and a compartment for the duration that the cx is on the
  // stack if non-null.
  if (cx) {
    mAutoRequest.emplace(cx);
  }
}

danger::AutoCxPusher::~AutoCxPusher()
{
  // Leave the request before popping.
  mAutoRequest.reset();

  // When we push a context, we may save the frame chain and pretend like we
  // haven't entered any compartment. This gets restored on Pop(), but we can
  // run into trouble if a Push/Pop are interleaved with a
  // JSAutoEnterCompartment. Make sure the compartment depth right before we
  // pop is the same as it was right after we pushed.
  MOZ_ASSERT_IF(mPushedContext, mCompartmentDepthOnEntry ==
                                js::GetEnterCompartmentDepth(mPushedContext));
  DebugOnly<JSContext*> stackTop;
  MOZ_ASSERT(mPushedContext == nsXPConnect::XPConnect()->GetCurrentJSContext());
  XPCJSRuntime::Get()->GetJSContextStack()->Pop();
  mScx = nullptr;
}

bool
danger::AutoCxPusher::IsStackTop() const
{
  uint32_t currentDepth = XPCJSRuntime::Get()->GetJSContextStack()->Count();
  MOZ_ASSERT(currentDepth >= mStackDepthAfterPush);
  return currentDepth == mStackDepthAfterPush;
}

} // namespace dom

AutoJSContext::AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : mCx(nullptr)
{
  JS::AutoSuppressGCAnalysis nogc;
  MOZ_ASSERT(!mCx, "mCx should not be initialized!");

  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  nsXPConnect *xpc = nsXPConnect::XPConnect();
  mCx = xpc->GetCurrentJSContext();

  if (!mCx) {
    mJSAPI.Init();
    mCx = mJSAPI.cx();
  }
}

AutoJSContext::operator JSContext*() const
{
  return mCx;
}

AutoSafeJSContext::AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
  : AutoJSAPI()
{
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  DebugOnly<bool> ok = Init(xpc::UnprivilegedJunkScope());
  MOZ_ASSERT(ok,
             "This is quite odd.  We should have crashed in the "
             "xpc::NativeGlobal() call if xpc::UnprivilegedJunkScope() "
             "returned null, and inited correctly otherwise!");
}

} // namespace mozilla
