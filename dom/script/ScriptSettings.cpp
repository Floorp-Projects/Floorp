/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"

#include <utility>
#include "LoadedScript.h"
#include "MainThreadUtils.h"
#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Exception.h"
#include "js/GCAPI.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/Warnings.h"
#include "js/Wrapper.h"
#include "js/friend/ErrorMessages.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptLoadRequest.h"
#include "mozilla/dom/WorkerCommon.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsGlobalWindowInner.h"
#include "nsIGlobalObject.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
#include "nsISupports.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "nscore.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

JSObject* SourceElementCallback(JSContext* aCx, JS::HandleValue aPrivateValue) {
  // NOTE: The result of this is only used by DevTools for matching sources, so
  // it is safe to silently ignore any errors and return nullptr for them.

  LoadedScript* script = static_cast<LoadedScript*>(aPrivateValue.toPrivate());

  if (!script->GetFetchOptions()) {
    return nullptr;
  }

  JS::Rooted<JS::Value> elementValue(aCx);
  {
    nsCOMPtr<Element> domElement = script->GetFetchOptions()->mElement;
    if (!domElement) {
      return nullptr;
    }

    JSObject* globalObject =
        domElement->OwnerDoc()->GetScopeObject()->GetGlobalJSObject();
    JSAutoRealm ar(aCx, globalObject);

    nsresult rv = nsContentUtils::WrapNative(aCx, domElement, &elementValue,
                                             /* aAllowWrapping = */ true);
    if (NS_FAILED(rv)) {
      return nullptr;
    }
  }

  return &elementValue.toObject();
}

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
    // any state on it.  Finally, we never made it to pushing ourselves onto the
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
  JS::SetSourceElementCallback(aCx, SourceElementCallback);

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
        if (NS_WARN_IF(!errorGlobal)) {
          // An exception may have been thrown on attempt to create a global
          // and now there is no realm from which to fetch the exception.
          // Give up.
          ClearException();
          return;
        }
      }
    }
  }
  MOZ_ASSERT(JS_IsGlobalObject(errorGlobal));
  JSAutoRealm ar(cx(), errorGlobal);
  JS::ExceptionStack exnStack(cx());
  JS::ErrorReportBuilder jsReport(cx());
  if (StealExceptionAndStack(&exnStack) &&
      jsReport.init(cx(), exnStack, JS::ErrorReportBuilder::WithSideEffects)) {
    if (mIsMainThread) {
      RefPtr<xpc::ErrorReport> xpcReport = new xpc::ErrorReport();

      RefPtr<nsGlobalWindowInner> inner = xpc::WindowOrNull(errorGlobal);
      bool isChrome =
          nsContentUtils::ObjectPrincipal(errorGlobal)->IsSystemPrincipal();
      xpcReport->Init(jsReport.report(), jsReport.toStringResult().c_str(),
                      isChrome, inner ? inner->WindowID() : 0);
      if (inner && jsReport.report()->errorNumber != JSMSG_OUT_OF_MEMORY) {
        JS::RootingContext* rcx = JS::RootingContext::get(cx());
        DispatchScriptErrorEvent(inner, rcx, xpcReport, exnStack.exception(),
                                 exnStack.stack());
      } else {
        JS::Rooted<JSObject*> stack(cx());
        JS::Rooted<JSObject*> stackGlobal(cx());
        xpc::FindExceptionStackForConsoleReport(inner, exnStack.exception(),
                                                exnStack.stack(), &stack,
                                                &stackGlobal);
        // This error is not associated with a specific window,
        // so omit the exception value to mitigate potential leaks.
        xpcReport->LogToConsoleWithStack(inner, JS::NothingHandleValue, stack,
                                         stackGlobal);
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
      JS::SetPendingExceptionStack(cx(), exnStack);
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
  return JS_GetPendingException(cx(), aVal);
}

bool AutoJSAPI::StealException(JS::MutableHandle<JS::Value> aVal) {
  JS::ExceptionStack exnStack(cx());
  if (!StealExceptionAndStack(&exnStack)) {
    return false;
  }
  aVal.set(exnStack.exception());
  return true;
}

bool AutoJSAPI::StealExceptionAndStack(JS::ExceptionStack* aExnStack) {
  MOZ_ASSERT_IF(mIsMainThread, IsStackTop());
  MOZ_ASSERT(HasException());
  MOZ_ASSERT(js::GetContextRealm(cx()));

  return JS::StealPendingExceptionStack(cx(), aExnStack);
}

#ifdef DEBUG
bool AutoJSAPI::IsStackTop() const {
  return ScriptSettingsStack::TopNonIncumbentScript() == this;
}
#endif  // DEBUG

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

AutoJSContext::AutoJSContext() : mCx(nullptr) {
  JS::AutoSuppressGCAnalysis nogc;
  MOZ_ASSERT(!mCx, "mCx should not be initialized!");
  MOZ_ASSERT(NS_IsMainThread());

  if (dom::IsJSAPIActive()) {
    mCx = dom::danger::GetJSContext();
  } else {
    mJSAPI.Init();
    mCx = mJSAPI.cx();
  }
}

AutoJSContext::operator JSContext*() const { return mCx; }

AutoSafeJSContext::AutoSafeJSContext() : AutoJSAPI() {
  MOZ_ASSERT(NS_IsMainThread());

  DebugOnly<bool> ok = Init(xpc::UnprivilegedJunkScope());
  MOZ_ASSERT(ok,
             "This is quite odd.  We should have crashed in the "
             "xpc::NativeGlobal() call if xpc::UnprivilegedJunkScope() "
             "returned null, and inited correctly otherwise!");
}

AutoSlowOperation::AutoSlowOperation() : mIsMainThread(NS_IsMainThread()) {
  if (mIsMainThread) {
    mScriptActivity.emplace(true);
  }
}

void AutoSlowOperation::CheckForInterrupt() {
  // For now we support only main thread!
  if (mIsMainThread) {
    // JS_CheckForInterrupt expects us to be in a realm, so we use a junk scope.
    // In principle, it doesn't matter which one we use, since we aren't really
    // running scripts here, and none of our interrupt callbacks can stop
    // scripts in a junk scope anyway. In practice, though, the privileged junk
    // scope is the same as the JSM global, and therefore always exists, while
    // the unprivileged junk scope is created lazily, and may not exist until we
    // try to use it. So we use the former for the sake of efficiency.
    dom::AutoJSAPI jsapi;
    MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
    JS_CheckForInterrupt(jsapi.cx());
  }
}

AutoAllowLegacyScriptExecution::AutoAllowLegacyScriptExecution() {
#ifdef DEBUG
  // no need to do that dance if we are off the main thread,
  // because we only assert if we are on the main thread!
  if (!NS_IsMainThread()) {
    return;
  }
  sAutoAllowLegacyScriptExecution++;
#endif
}

AutoAllowLegacyScriptExecution::~AutoAllowLegacyScriptExecution() {
#ifdef DEBUG
  // no need to do that dance if we are off the main thread,
  // because we only assert if we are on the main thread!
  if (!NS_IsMainThread()) {
    return;
  }
  sAutoAllowLegacyScriptExecution--;
  MOZ_ASSERT(sAutoAllowLegacyScriptExecution >= 0,
             "how can the stack guard produce a value less than 0?");
#endif
}

int AutoAllowLegacyScriptExecution::sAutoAllowLegacyScriptExecution = 0;

/*static*/
bool AutoAllowLegacyScriptExecution::IsAllowed() {
  return sAutoAllowLegacyScriptExecution > 0;
}

}  // namespace mozilla
