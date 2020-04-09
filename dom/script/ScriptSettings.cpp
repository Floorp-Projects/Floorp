/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/WorkerPrivate.h"

#include "jsapi.h"
#include "js/Warnings.h"  // JS::{Get,}WarningReporter
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

namespace mozilla {
namespace dom {

static MOZ_THREAD_LOCAL(ScriptSettingsStackEntry*) sScriptSettingsTLS;

class ScriptSettingsStack {
 public:
  static ScriptSettingsStackEntry* Top() { return sScriptSettingsTLS.get(); }

  static void Push(ScriptSettingsStackEntry* aEntry) {
    MOZ_ASSERT(!aEntry->mOlder);
    // Whenever JSAPI use is disabled, the next stack entry pushed must
    // not be an AutoIncumbentScript.
    MOZ_ASSERT_IF(!Top() || Top()->NoJSAPI(), !aEntry->IsIncumbentScript());
    // Whenever the top entry is not an incumbent canidate, the next stack entry
    // pushed must not be an AutoIncumbentScript.
    MOZ_ASSERT_IF(Top() && !Top()->IsIncumbentCandidate(),
                  !aEntry->IsIncumbentScript());

    aEntry->mOlder = Top();
    sScriptSettingsTLS.set(aEntry);
  }

  static void Pop(ScriptSettingsStackEntry* aEntry) {
    MOZ_ASSERT(aEntry == Top());
    sScriptSettingsTLS.set(aEntry->mOlder);
  }

  static nsIGlobalObject* IncumbentGlobal() {
    ScriptSettingsStackEntry* entry = Top();
    while (entry) {
      if (entry->IsIncumbentCandidate()) {
        return entry->mGlobalObject;
      }
      entry = entry->mOlder;
    }
    return nullptr;
  }

  static ScriptSettingsStackEntry* EntryPoint() {
    ScriptSettingsStackEntry* entry = Top();
    while (entry) {
      if (entry->IsEntryCandidate()) {
        return entry;
      }
      entry = entry->mOlder;
    }
    return nullptr;
  }

  static nsIGlobalObject* EntryGlobal() {
    ScriptSettingsStackEntry* entry = EntryPoint();
    if (!entry) {
      return nullptr;
    }
    return entry->mGlobalObject;
  }

#ifdef DEBUG
  static ScriptSettingsStackEntry* TopNonIncumbentScript() {
    ScriptSettingsStackEntry* entry = Top();
    while (entry) {
      if (!entry->IsIncumbentScript()) {
        return entry;
      }
      entry = entry->mOlder;
    }
    return nullptr;
  }
#endif  // DEBUG
};

static unsigned long gRunToCompletionListeners = 0;

void UseEntryScriptProfiling() {
  MOZ_ASSERT(NS_IsMainThread());
  ++gRunToCompletionListeners;
}

void UnuseEntryScriptProfiling() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gRunToCompletionListeners > 0);
  --gRunToCompletionListeners;
}

void InitScriptSettings() {
  bool success = sScriptSettingsTLS.init();
  if (!success) {
    MOZ_CRASH();
  }

  sScriptSettingsTLS.set(nullptr);
}

void DestroyScriptSettings() {
  MOZ_ASSERT(sScriptSettingsTLS.get() == nullptr);
}

ScriptSettingsStackEntry::ScriptSettingsStackEntry(nsIGlobalObject* aGlobal,
                                                   Type aType)
    : mGlobalObject(aGlobal), mType(aType), mOlder(nullptr) {
  MOZ_ASSERT_IF(IsIncumbentCandidate() && !NoJSAPI(), mGlobalObject);
  MOZ_ASSERT(!mGlobalObject || mGlobalObject->HasJSGlobal(),
             "Must have an actual JS global for the duration on the stack");
  MOZ_ASSERT(
      !mGlobalObject ||
          JS_IsGlobalObject(mGlobalObject->GetGlobalJSObjectPreserveColor()),
      "No outer windows allowed");
}

ScriptSettingsStackEntry::~ScriptSettingsStackEntry() {
  // We must have an actual JS global for the entire time this is on the stack.
  MOZ_ASSERT_IF(mGlobalObject, mGlobalObject->HasJSGlobal());
}

// If the entry or incumbent global ends up being something that the subject
// principal doesn't subsume, we don't want to use it. This never happens on
// the web, but can happen with asymmetric privilege relationships (i.e.
// ExpandedPrincipal and System Principal).
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
static nsIGlobalObject* ClampToSubject(nsIGlobalObject* aGlobalOrNull) {
  if (!aGlobalOrNull || !NS_IsMainThread()) {
    return aGlobalOrNull;
  }

  nsIPrincipal* globalPrin = aGlobalOrNull->PrincipalOrNull();
  NS_ENSURE_TRUE(globalPrin, GetCurrentGlobal());
  if (!nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller()
           ->SubsumesConsideringDomain(globalPrin)) {
    return GetCurrentGlobal();
  }

  return aGlobalOrNull;
}

nsIGlobalObject* GetEntryGlobal() {
  return ClampToSubject(ScriptSettingsStack::EntryGlobal());
}

Document* GetEntryDocument() {
  nsIGlobalObject* global = GetEntryGlobal();
  nsCOMPtr<nsPIDOMWindowInner> entryWin = do_QueryInterface(global);

  return entryWin ? entryWin->GetExtantDoc() : nullptr;
}

nsIGlobalObject* GetIncumbentGlobal() {
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry global on the stack, and therefore no incumbent
  // global either.
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::EntryGlobal() == nullptr);
    return nullptr;
  }

  // See what the JS engine has to say. If we've got a scripted caller
  // override in place, the JS engine will lie to us and pretend that
  // there's nothing on the JS stack, which will cause us to check the
  // incumbent script stack below.
  if (JSObject* global = JS::GetScriptedCallerGlobal(cx)) {
    return ClampToSubject(xpc::NativeGlobal(global));
  }

  // Ok, nothing from the JS engine. Let's use whatever's on the
  // explicit stack.
  return ClampToSubject(ScriptSettingsStack::IncumbentGlobal());
}

nsIGlobalObject* GetCurrentGlobal() {
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    return nullptr;
  }

  JSObject* global = JS::CurrentGlobalOrNull(cx);
  if (!global) {
    return nullptr;
  }

  return xpc::NativeGlobal(global);
}

nsIPrincipal* GetWebIDLCallerPrincipal() {
  MOZ_ASSERT(NS_IsMainThread());
  ScriptSettingsStackEntry* entry = ScriptSettingsStack::EntryPoint();

  // If we have an entry point that is not NoJSAPI, we know it must be an
  // AutoEntryScript.
  if (!entry || entry->NoJSAPI()) {
    return nullptr;
  }
  AutoEntryScript* aes = static_cast<AutoEntryScript*>(entry);

  return aes->mWebIDLCallerPrincipal;
}

bool IsJSAPIActive() {
  ScriptSettingsStackEntry* topEntry = ScriptSettingsStack::Top();
  return topEntry && !topEntry->NoJSAPI();
}

namespace danger {
JSContext* GetJSContext() { return CycleCollectedJSContext::Get()->Context(); }
}  // namespace danger

JS::RootingContext* RootingCx() {
  return CycleCollectedJSContext::Get()->RootingCx();
}

AutoJSAPI::AutoJSAPI()
    : ScriptSettingsStackEntry(nullptr, eJSAPI),
      mCx(nullptr),
      mIsMainThread(false)  // For lack of anything better
{}

AutoJSAPI::~AutoJSAPI() {
  if (!mCx) {
    // No need to do anything here: we never managed to Init, so can't have an
    // exception on our (nonexistent) JSContext.  We also don't need to restore
    // any state on it.  Finally, we never made it to pushing outselves onto the
    // ScriptSettingsStack, so shouldn't pop.
    MOZ_ASSERT(ScriptSettingsStack::Top() != this);
    return;
  }

  ReportException();

  if (mOldWarningReporter.isSome()) {
    JS::SetWarningReporter(cx(), mOldWarningReporter.value());
  }

  ScriptSettingsStack::Pop(this);
}

void WarningOnlyErrorReporter(JSContext* aCx, JSErrorReport* aRep);

void AutoJSAPI::InitInternal(nsIGlobalObject* aGlobalObject, JSObject* aGlobal,
                             JSContext* aCx, bool aIsMainThread) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aCx == danger::GetJSContext());
  MOZ_ASSERT(aIsMainThread == NS_IsMainThread());
  MOZ_ASSERT(bool(aGlobalObject) == bool(aGlobal));
  MOZ_ASSERT_IF(aGlobalObject,
                aGlobalObject->GetGlobalJSObjectPreserveColor() == aGlobal);
#ifdef DEBUG
  bool haveException = JS_IsExceptionPending(aCx);
#endif  // DEBUG

  mCx = aCx;
  mIsMainThread = aIsMainThread;
  if (aGlobal) {
    JS::AssertObjectIsNotGray(aGlobal);
  }
  mAutoNullableRealm.emplace(mCx, aGlobal);
  mGlobalObject = aGlobalObject;

  ScriptSettingsStack::Push(this);

  mOldWarningReporter.emplace(JS::GetWarningReporter(aCx));

  JS::SetWarningReporter(aCx, WarningOnlyErrorReporter);

#ifdef DEBUG
  if (haveException) {
    JS::Rooted<JS::Value> exn(aCx);
    JS_GetPendingException(aCx, &exn);

    JS_ClearPendingException(aCx);
    if (exn.isObject()) {
      JS::Rooted<JSObject*> exnObj(aCx, &exn.toObject());

      // Make sure we can actually read things from it.  This UncheckedUwrap is
      // safe because we're only getting data for a debug printf.  In
      // particular, we do not expose this data to anyone, which is very
      // important; otherwise it could be a cross-origin information leak.
      exnObj = js::UncheckedUnwrap(exnObj);
      JSAutoRealm ar(aCx, exnObj);

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

      if (!JS_GetProperty(aCx, exnObj, "name", &tmp) || !name.init(aCx, tmp)) {
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
#endif  // DEBUG
}

AutoJSAPI::AutoJSAPI(nsIGlobalObject* aGlobalObject, bool aIsMainThread,
                     Type aType)
    : ScriptSettingsStackEntry(aGlobalObject, aType),
      mIsMainThread(aIsMainThread) {
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aGlobalObject->HasJSGlobal(), "Must have a JS global");
  MOZ_ASSERT(aIsMainThread == NS_IsMainThread());

  InitInternal(aGlobalObject, aGlobalObject->GetGlobalJSObject(),
               danger::GetJSContext(), aIsMainThread);
}

void AutoJSAPI::Init() {
  MOZ_ASSERT(!mCx, "An AutoJSAPI should only be initialised once");

  InitInternal(/* aGlobalObject */ nullptr, /* aGlobal */ nullptr,
               danger::GetJSContext(), NS_IsMainThread());
}

bool AutoJSAPI::Init(nsIGlobalObject* aGlobalObject, JSContext* aCx) {
  MOZ_ASSERT(!mCx, "An AutoJSAPI should only be initialised once");
  MOZ_ASSERT(aCx);

  if (NS_WARN_IF(!aGlobalObject)) {
    return false;
  }

  JSObject* global = aGlobalObject->GetGlobalJSObject();
  if (NS_WARN_IF(!global)) {
    return false;
  }

  InitInternal(aGlobalObject, global, aCx, NS_IsMainThread());
  return true;
}

bool AutoJSAPI::Init(nsIGlobalObject* aGlobalObject) {
  return Init(aGlobalObject, danger::GetJSContext());
}

bool AutoJSAPI::Init(JSObject* aObject) {
  MOZ_ASSERT(!js::IsCrossCompartmentWrapper(aObject));
  return Init(xpc::NativeGlobal(aObject));
}

bool AutoJSAPI::Init(nsPIDOMWindowInner* aWindow, JSContext* aCx) {
  return Init(nsGlobalWindowInner::Cast(aWindow), aCx);
}

bool AutoJSAPI::Init(nsPIDOMWindowInner* aWindow) {
  return Init(nsGlobalWindowInner::Cast(aWindow));
}

bool AutoJSAPI::Init(nsGlobalWindowInner* aWindow, JSContext* aCx) {
  return Init(static_cast<nsIGlobalObject*>(aWindow), aCx);
}

bool AutoJSAPI::Init(nsGlobalWindowInner* aWindow) {
  return Init(static_cast<nsIGlobalObject*>(aWindow));
}

// Even with autoJSAPIOwnsErrorReporting, the JS engine still sends warning
// reports to the JSErrorReporter as soon as they are generated. These go
// directly to the console, so we can handle them easily here.
//
// Eventually, SpiderMonkey will have a special-purpose callback for warnings
// only.
void WarningOnlyErrorReporter(JSContext* aCx, JSErrorReport* aRep) {
  MOZ_ASSERT(aRep->isWarning());
  if (!NS_IsMainThread()) {
    // Reporting a warning on workers is a bit complicated because we have to
    // climb our parent chain until we get to the main thread.  So go ahead and
    // just go through the worker or worklet ReportError codepath here.
    //
    // That said, it feels like we should be able to short-circuit things a bit
    // here by posting an appropriate runnable to the main thread directly...
    // Worth looking into sometime.
    CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::GetFor(aCx);
    MOZ_ASSERT(ccjscx);

    ccjscx->ReportError(aRep, JS::ConstUTF8CharsZ());
    return;
  }

  RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();
  nsGlobalWindowInner* win = xpc::CurrentWindowOrNull(aCx);
  xpcReport->Init(aRep, nullptr, nsContentUtils::IsSystemCaller(aCx),
                  win ? win->WindowID() : 0);
  xpcReport->LogToConsole();
}

void AutoJSAPI::ReportException() {
  if (!HasException()) {
    return;
  }

  // AutoJSAPI uses a JSAutoNullableRealm, and may be in a null realm
  // when the destructor is called. However, the JS engine requires us
  // to be in a realm when we fetch the pending exception. In this case,
  // we enter the privileged junk scope and don't dispatch any error events.
  JS::Rooted<JSObject*> errorGlobal(cx(), JS::CurrentGlobalOrNull(cx()));
  if (!errorGlobal) {
    if (mIsMainThread) {
      errorGlobal = xpc::PrivilegedJunkScope();
    } else {
      errorGlobal = GetCurrentThreadWorkerGlobal();
      if (!errorGlobal) {
        // We might be reporting an error in debugger code that ran before the
        // worker's global was created. Use the debugger global instead.
        errorGlobal = GetCurrentThreadWorkerDebuggerGlobal();
      }
    }
  }
  MOZ_ASSERT(JS_IsGlobalObject(errorGlobal));
  JSAutoRealm ar(cx(), errorGlobal);
  JS::Rooted<JS::Value> exn(cx());
  JS::Rooted<JSObject*> exnStack(cx());
  js::ErrorReport jsReport(cx());
  if (StealExceptionAndStack(&exn, &exnStack) &&
      jsReport.init(cx(), exn, js::ErrorReport::WithSideEffects, exnStack)) {
    if (mIsMainThread) {
      RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();

      RefPtr<nsGlobalWindowInner> inner = xpc::WindowOrNull(errorGlobal);
      bool isChrome =
          nsContentUtils::ObjectPrincipal(errorGlobal)->IsSystemPrincipal();
      xpcReport->Init(jsReport.report(), jsReport.toStringResult().c_str(),
                      isChrome, inner ? inner->WindowID() : 0);
      if (inner && jsReport.report()->errorNumber != JSMSG_OUT_OF_MEMORY) {
        JS::RootingContext* rcx = JS::RootingContext::get(cx());
        DispatchScriptErrorEvent(inner, rcx, xpcReport, exn, exnStack);
      } else {
        JS::Rooted<JSObject*> stack(cx());
        JS::Rooted<JSObject*> stackGlobal(cx());
        xpc::FindExceptionStackForConsoleReport(inner, exn, exnStack, &stack,
                                                &stackGlobal);
        xpcReport->LogToConsoleWithStack(stack, stackGlobal);
      }
    } else {
      // On a worker or worklet, we just use the error reporting mechanism and
      // don't bother with xpc::ErrorReport.  This will ensure that all the
      // right worker events (which are a lot more complicated than in the
      // window case) get fired.
      CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::GetFor(cx());
      MOZ_ASSERT(ccjscx);
      // Before invoking ReportError, put the exception back on the context,
      // because it may want to put it in its error events and has no other way
      // to get hold of it.  After we invoke ReportError, clear the exception on
      // cx(), just in case ReportError didn't.
      JS::SetPendingExceptionAndStack(cx(), exn, exnStack);
      ccjscx->ReportError(jsReport.report(), jsReport.toStringResult());
      ClearException();
    }
  } else {
    NS_WARNING("OOMed while acquiring uncaught exception from JSAPI");
    ClearException();
  }
}

bool AutoJSAPI::PeekException(JS::MutableHandle<JS::Value> aVal) {
  MOZ_ASSERT_IF(mIsMainThread, IsStackTop());
  MOZ_ASSERT(HasException());
  MOZ_ASSERT(js::GetContextRealm(cx()));
  if (!JS_GetPendingException(cx(), aVal)) {
    return false;
  }
  return true;
}

bool AutoJSAPI::StealException(JS::MutableHandle<JS::Value> aVal) {
  JS::Rooted<JSObject*> stack(cx());
  return StealExceptionAndStack(aVal, &stack);
}

bool AutoJSAPI::StealExceptionAndStack(JS::MutableHandle<JS::Value> aVal,
                                       JS::MutableHandle<JSObject*> aStack) {
  if (!PeekException(aVal)) {
    return false;
  }

  JS::ExceptionStack exnStack(cx());
  if (!JS::StealPendingExceptionStack(cx(), &exnStack)) {
    return false;
  }

  aStack.set(exnStack.stack());
  return true;
}

#ifdef DEBUG
bool AutoJSAPI::IsStackTop() const {
  return ScriptSettingsStack::TopNonIncumbentScript() == this;
}
#endif  // DEBUG

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 const char* aReason, bool aIsMainThread)
    : AutoJSAPI(aGlobalObject, aIsMainThread, eEntryScript),
      mWebIDLCallerPrincipal(nullptr)
      // This relies on us having a cx() because the AutoJSAPI constructor
      // already ran.
      ,
      mCallerOverride(cx())
#ifdef MOZ_GECKO_PROFILER
      ,
      mAutoProfilerLabel(
          "", aReason, JS::ProfilingCategoryPair::JS,
          uint32_t(js::ProfilingStackFrame::Flags::RELEVANT_FOR_JS))
#endif
      ,
      mJSThreadExecution(aGlobalObject, aIsMainThread) {
  MOZ_ASSERT(aGlobalObject);

  if (aIsMainThread) {
    if (gRunToCompletionListeners > 0) {
      mDocShellEntryMonitor.emplace(cx(), aReason);
    }
    mScriptActivity.emplace(true);
  }
}

AutoEntryScript::AutoEntryScript(JSObject* aObject, const char* aReason,
                                 bool aIsMainThread)
    : AutoEntryScript(xpc::NativeGlobal(aObject), aReason, aIsMainThread) {
  // xpc::NativeGlobal uses JS::GetNonCCWObjectGlobal, which asserts that
  // aObject is not a CCW.
}

AutoEntryScript::~AutoEntryScript() = default;

AutoEntryScript::DocshellEntryMonitor::DocshellEntryMonitor(JSContext* aCx,
                                                            const char* aReason)
    : JS::dbg::AutoEntryMonitor(aCx), mReason(aReason) {}

void AutoEntryScript::DocshellEntryMonitor::Entry(
    JSContext* aCx, JSFunction* aFunction, JSScript* aScript,
    JS::Handle<JS::Value> aAsyncStack, const char* aAsyncCause) {
  JS::Rooted<JSFunction*> rootedFunction(aCx);
  if (aFunction) {
    rootedFunction = aFunction;
  }
  JS::Rooted<JSScript*> rootedScript(aCx);
  if (aScript) {
    rootedScript = aScript;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = xpc::CurrentWindowOrNull(aCx);
  if (!window || !window->GetDocShell() ||
      !window->GetDocShell()->GetRecordProfileTimelineMarkers()) {
    return;
  }

  nsCOMPtr<nsIDocShell> docShellForJSRunToCompletion = window->GetDocShell();

  nsAutoJSString functionName;
  if (rootedFunction) {
    JS::Rooted<JSString*> displayId(aCx,
                                    JS_GetFunctionDisplayId(rootedFunction));
    if (displayId) {
      if (!functionName.init(aCx, displayId)) {
        JS_ClearPendingException(aCx);
        return;
      }
    }
  }

  nsString filename;
  uint32_t lineNumber = 0;
  if (!rootedScript) {
    rootedScript = JS_GetFunctionScript(aCx, rootedFunction);
  }
  if (rootedScript) {
    filename = NS_ConvertUTF8toUTF16(JS_GetScriptFilename(rootedScript));
    lineNumber = JS_GetScriptBaseLineNumber(aCx, rootedScript);
  }

  if (!filename.IsEmpty() || !functionName.IsEmpty()) {
    docShellForJSRunToCompletion->NotifyJSRunToCompletionStart(
        mReason, functionName, filename, lineNumber, aAsyncStack, aAsyncCause);
  }
}

void AutoEntryScript::DocshellEntryMonitor::Exit(JSContext* aCx) {
  nsCOMPtr<nsPIDOMWindowInner> window = xpc::CurrentWindowOrNull(aCx);
  // Not really worth checking GetRecordProfileTimelineMarkers here.
  if (window && window->GetDocShell()) {
    nsCOMPtr<nsIDocShell> docShellForJSRunToCompletion = window->GetDocShell();
    docShellForJSRunToCompletion->NotifyJSRunToCompletionStop();
  }
}

AutoIncumbentScript::AutoIncumbentScript(nsIGlobalObject* aGlobalObject)
    : ScriptSettingsStackEntry(aGlobalObject, eIncumbentScript),
      mCallerOverride(nsContentUtils::GetCurrentJSContext()) {
  ScriptSettingsStack::Push(this);
}

AutoIncumbentScript::~AutoIncumbentScript() { ScriptSettingsStack::Pop(this); }

AutoNoJSAPI::AutoNoJSAPI(JSContext* aCx)
    : ScriptSettingsStackEntry(nullptr, eNoJSAPI),
      JSAutoNullableRealm(aCx, nullptr),
      mCx(aCx) {
  // Make sure we don't seem to have an incumbent global due to
  // whatever script is running right now.
  JS::HideScriptedCaller(aCx);

  // Make sure the fallback GetIncumbentGlobal() behavior and
  // GetEntryGlobal() both return null.
  ScriptSettingsStack::Push(this);
}

AutoNoJSAPI::~AutoNoJSAPI() {
  ScriptSettingsStack::Pop(this);
  JS::UnhideScriptedCaller(mCx);
}

}  // namespace dom

AutoJSContext::AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
    : mCx(nullptr) {
  JS::AutoSuppressGCAnalysis nogc;
  MOZ_ASSERT(!mCx, "mCx should not be initialized!");
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  if (dom::IsJSAPIActive()) {
    mCx = dom::danger::GetJSContext();
  } else {
    mJSAPI.Init();
    mCx = mJSAPI.cx();
  }
}

AutoJSContext::operator JSContext*() const { return mCx; }

AutoSafeJSContext::AutoSafeJSContext(
    MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
    : AutoJSAPI() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_GUARD_OBJECT_NOTIFIER_INIT;

  DebugOnly<bool> ok = Init(xpc::UnprivilegedJunkScope());
  MOZ_ASSERT(ok,
             "This is quite odd.  We should have crashed in the "
             "xpc::NativeGlobal() call if xpc::UnprivilegedJunkScope() "
             "returned null, and inited correctly otherwise!");
}

AutoSlowOperation::AutoSlowOperation(
    MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM_IN_IMPL)
    : mIsMainThread(NS_IsMainThread()) {
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  if (mIsMainThread) {
    mScriptActivity.emplace(true);
  }
}

void AutoSlowOperation::CheckForInterrupt() {
  // For now we support only main thread!
  if (mIsMainThread) {
    // JS_CheckForInterrupt expects us to be in a realm.
    AutoJSAPI jsapi;
    if (jsapi.Init(xpc::UnprivilegedJunkScope())) {
      JS_CheckForInterrupt(jsapi.cx());
    }
  }
}

}  // namespace mozilla
