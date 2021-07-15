/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AutoEntryScript.h"

#include <stdint.h>
#include <utility>
#include "js/ProfilingCategory.h"
#include "js/ProfilingStack.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIDocShell.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsPIDOMWindowInlines.h"
#include "nsString.h"
#include "xpcpublic.h"

namespace mozilla::dom {

namespace {
// Assert if it's not safe to run script. The helper class
// AutoAllowLegacyScriptExecution allows to allow-list
// legacy cases where it's actually not safe to run script.
#ifdef DEBUG
void AssertIfNotSafeToRunScript() {
  // if it's safe to run script, then there is nothing to do here.
  if (nsContentUtils::IsSafeToRunScript()) {
    return;
  }

  // auto allowing legacy script execution is fine for now.
  if (AutoAllowLegacyScriptExecution::IsAllowed()) {
    return;
  }

  MOZ_ASSERT(false, "is it safe to run script?");
}
#endif

static unsigned long gRunToCompletionListeners = 0;

}  // namespace

void UseEntryScriptProfiling() {
  MOZ_ASSERT(NS_IsMainThread());
  ++gRunToCompletionListeners;
}

void UnuseEntryScriptProfiling() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gRunToCompletionListeners > 0);
  --gRunToCompletionListeners;
}

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 const char* aReason, bool aIsMainThread)
    : AutoJSAPI(aGlobalObject, aIsMainThread, eEntryScript),
      mWebIDLCallerPrincipal(nullptr)
      // This relies on us having a cx() because the AutoJSAPI constructor
      // already ran.
      ,
      mCallerOverride(cx()),
      mAutoProfilerLabel(
          "", aReason, JS::ProfilingCategoryPair::JS,
          uint32_t(js::ProfilingStackFrame::Flags::RELEVANT_FOR_JS)),
      mJSThreadExecution(aGlobalObject, aIsMainThread) {
  MOZ_ASSERT(aGlobalObject);

  if (aIsMainThread) {
#ifdef DEBUG
    AssertIfNotSafeToRunScript();
#endif
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
    CopyUTF8toUTF16(MakeStringSpan(JS_GetScriptFilename(rootedScript)),
                    filename);
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

}  // namespace mozilla::dom
