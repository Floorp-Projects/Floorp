/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is not a generated file. It contains common utility functions
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsJSUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsIScriptSecurityManager.h"
#include "nsPIDOMWindow.h"
#include "GeckoProfiler.h"
#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext
#include "nsJSPrincipals.h"
#include "xpcpublic.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla::dom;

bool
nsJSUtils::GetCallingLocation(JSContext* aContext, const char* *aFilename,
                              uint32_t* aLineno)
{
  JS::AutoFilename filename;
  unsigned lineno = 0;

  if (!JS::DescribeScriptedCaller(aContext, &filename, &lineno)) {
    return false;
  }

  *aFilename = filename.get();
  *aLineno = lineno;

  return true;
}

nsIScriptGlobalObject *
nsJSUtils::GetStaticScriptGlobal(JSObject* aObj)
{
  if (!aObj)
    return nullptr;
  return xpc::WindowGlobalOrNull(aObj);
}

nsIScriptContext *
nsJSUtils::GetStaticScriptContext(JSObject* aObj)
{
  nsIScriptGlobalObject *nativeGlobal = GetStaticScriptGlobal(aObj);
  if (!nativeGlobal)
    return nullptr;

  return nativeGlobal->GetScriptContext();
}

uint64_t
nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(JSContext *aContext)
{
  if (!aContext)
    return 0;

  uint64_t innerWindowID = 0;

  JSObject *jsGlobal = JS::CurrentGlobalOrNull(aContext);
  if (jsGlobal) {
    nsIScriptGlobalObject *scriptGlobal = GetStaticScriptGlobal(jsGlobal);
    if (scriptGlobal) {
      nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(scriptGlobal);
      if (win)
        innerWindowID = win->WindowID();
    }
  }

  return innerWindowID;
}

void
nsJSUtils::ReportPendingException(JSContext *aContext)
{
  if (JS_IsExceptionPending(aContext)) {
    bool saved = JS_SaveFrameChain(aContext);
    {
      // JS_SaveFrameChain set the compartment of aContext to null, so we need
      // to enter a compartment.  The question is, which one? We don't want to
      // enter the original compartment of aContext (or the compartment of the
      // current exception on aContext, for that matter) because when we
      // JS_ReportPendingException the JS engine can try to duck-type the
      // exception and produce a JSErrorReport.  It will then pass that
      // JSErrorReport to the error reporter on aContext, which might expose
      // information from it to script via onerror handlers.  So it's very
      // important that the duck typing happen in the same compartment as the
      // onerror handler.  In practice, that's the compartment of the window (or
      // otherwise default global) of aContext, so use that here.
      nsIScriptContext* scx = GetScriptContextFromJSContext(aContext);
      JS::Rooted<JSObject*> scope(aContext);
      scope = scx ? scx->GetWindowProxy() : nullptr;
      if (!scope) {
        // The SafeJSContext has no default object associated with it.
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aContext == nsContentUtils::GetSafeJSContext());
        scope = xpc::UnprivilegedJunkScope(); // Usage approved by bholley
      }
      JSAutoCompartment ac(aContext, scope);
      JS_ReportPendingException(aContext);
    }
    if (saved) {
      JS_RestoreFrameChain(aContext);
    }
  }
}

nsresult
nsJSUtils::CompileFunction(AutoJSAPI& jsapi,
                           JS::AutoObjectVector& aScopeChain,
                           JS::CompileOptions& aOptions,
                           const nsACString& aName,
                           uint32_t aArgCount,
                           const char** aArgArray,
                           const nsAString& aBody,
                           JSObject** aFunctionObject)
{
  MOZ_ASSERT(jsapi.OwnsErrorReporting());
  JSContext* cx = jsapi.cx();
  MOZ_ASSERT(js::GetEnterCompartmentDepth(cx) > 0);
  MOZ_ASSERT_IF(aScopeChain.length() != 0,
                js::IsObjectInContextCompartment(aScopeChain[0], cx));
  MOZ_ASSERT_IF(aOptions.versionSet, aOptions.version != JSVERSION_UNKNOWN);
  mozilla::DebugOnly<nsIScriptContext*> ctx = GetScriptContextFromJSContext(cx);
  MOZ_ASSERT_IF(ctx, ctx->IsContextInitialized());

  // Do the junk Gecko is supposed to do before calling into JSAPI.
  for (size_t i = 0; i < aScopeChain.length(); ++i) {
    JS::ExposeObjectToActiveJS(aScopeChain[i]);
  }

  // Compile.
  JS::Rooted<JSFunction*> fun(cx);
  if (!JS::CompileFunction(cx, aScopeChain, aOptions,
                           PromiseFlatCString(aName).get(),
                           aArgCount, aArgArray,
                           PromiseFlatString(aBody).get(),
                           aBody.Length(), &fun))
  {
    return NS_ERROR_FAILURE;
  }

  *aFunctionObject = JS_GetFunctionObject(fun);
  return NS_OK;
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          const nsAString& aScript,
                          JS::Handle<JSObject*> aEvaluationGlobal,
                          JS::CompileOptions& aCompileOptions,
                          const EvaluateOptions& aEvaluateOptions,
                          JS::MutableHandle<JS::Value> aRetValue)
{
  const nsPromiseFlatString& flatScript = PromiseFlatString(aScript);
  JS::SourceBufferHolder srcBuf(flatScript.get(), aScript.Length(),
                                JS::SourceBufferHolder::NoOwnership);
  return EvaluateString(aCx, srcBuf, aEvaluationGlobal, aCompileOptions,
                        aEvaluateOptions, aRetValue, nullptr);
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          JS::SourceBufferHolder& aSrcBuf,
                          JS::Handle<JSObject*> aEvaluationGlobal,
                          JS::CompileOptions& aCompileOptions,
                          const EvaluateOptions& aEvaluateOptions,
                          JS::MutableHandle<JS::Value> aRetValue,
                          void **aOffThreadToken)
{
  PROFILER_LABEL("nsJSUtils", "EvaluateString",
    js::ProfileEntry::Category::JS);

  MOZ_ASSERT_IF(aCompileOptions.versionSet,
                aCompileOptions.version != JSVERSION_UNKNOWN);
  MOZ_ASSERT_IF(aEvaluateOptions.coerceToString, !aCompileOptions.noScriptRval);
  MOZ_ASSERT_IF(!aEvaluateOptions.reportUncaught, !aCompileOptions.noScriptRval);
  // Note that the above assert means that if aCompileOptions.noScriptRval then
  // also aEvaluateOptions.reportUncaught.
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());
  MOZ_ASSERT(aSrcBuf.get());
  MOZ_ASSERT(js::GetGlobalForObjectCrossCompartment(aEvaluationGlobal) ==
             aEvaluationGlobal);
  MOZ_ASSERT_IF(aOffThreadToken, aCompileOptions.noScriptRval);

  // Unfortunately, the JS engine actually compiles scripts with a return value
  // in a different, less efficient way.  Furthermore, it can't JIT them in many
  // cases.  So we need to be explicitly told whether the caller cares about the
  // return value.  Callers can do this by calling the other overload of
  // EvaluateString() which calls this function with
  // aCompileOptions.noScriptRval set to true.
  aRetValue.setUndefined();

  nsresult rv = NS_OK;

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(ssm->ScriptAllowed(aEvaluationGlobal), NS_OK);

  mozilla::Maybe<AutoDontReportUncaught> dontReport;
  if (!aEvaluateOptions.reportUncaught) {
    // We need to prevent AutoLastFrameCheck from reporting and clearing
    // any pending exceptions.
    dontReport.emplace(aCx);
  }

  bool ok = true;
  // Scope the JSAutoCompartment so that we can later wrap the return value
  // into the caller's cx.
  {
    JSAutoCompartment ac(aCx, aEvaluationGlobal);

    // Now make sure to wrap the scope chain into the right compartment.
    JS::AutoObjectVector scopeChain(aCx);
    if (!scopeChain.reserve(aEvaluateOptions.scopeChain.length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < aEvaluateOptions.scopeChain.length(); ++i) {
      JS::ExposeObjectToActiveJS(aEvaluateOptions.scopeChain[i]);
      scopeChain.infallibleAppend(aEvaluateOptions.scopeChain[i]);
      if (!JS_WrapObject(aCx, scopeChain[i])) {
        ok = false;
        break;
      }
    }

    if (ok && aOffThreadToken) {
      JS::Rooted<JSScript*>
        script(aCx, JS::FinishOffThreadScript(aCx, JS_GetRuntime(aCx), *aOffThreadToken));
      *aOffThreadToken = nullptr; // Mark the token as having been finished.
      if (script) {
        ok = JS_ExecuteScript(aCx, scopeChain, script);
      } else {
        ok = false;
      }
    } else if (ok) {
      if (!aCompileOptions.noScriptRval) {
        ok = JS::Evaluate(aCx, scopeChain, aCompileOptions, aSrcBuf, aRetValue);
      } else {
        ok = JS::Evaluate(aCx, scopeChain, aCompileOptions, aSrcBuf);
      }
    }

    if (ok && aEvaluateOptions.coerceToString && !aRetValue.isUndefined()) {
      JS::Rooted<JS::Value> value(aCx, aRetValue);
      JSString* str = JS::ToString(aCx, value);
      ok = !!str;
      aRetValue.set(ok ? JS::StringValue(str) : JS::UndefinedValue());
    }
  }

  if (!ok) {
    if (aEvaluateOptions.reportUncaught) {
      ReportPendingException(aCx);
      if (!aCompileOptions.noScriptRval) {
        aRetValue.setUndefined();
      }
    } else {
      rv = JS_IsExceptionPending(aCx) ? NS_ERROR_FAILURE
                                      : NS_ERROR_OUT_OF_MEMORY;
      JS::Rooted<JS::Value> exn(aCx);
      JS_GetPendingException(aCx, &exn);
      MOZ_ASSERT(!aCompileOptions.noScriptRval); // we asserted this on entry
      aRetValue.set(exn);
      JS_ClearPendingException(aCx);
    }
  }

  // Wrap the return value into whatever compartment aCx was in.
  if (!aCompileOptions.noScriptRval) {
    if (!JS_WrapValue(aCx, aRetValue)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return rv;
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          JS::SourceBufferHolder& aSrcBuf,
                          JS::Handle<JSObject*> aEvaluationGlobal,
                          JS::CompileOptions& aCompileOptions,
                          const EvaluateOptions& aEvaluateOptions,
                          JS::MutableHandle<JS::Value> aRetValue)
{
  return EvaluateString(aCx, aSrcBuf, aEvaluationGlobal, aCompileOptions,
                        aEvaluateOptions, aRetValue, nullptr);
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          const nsAString& aScript,
                          JS::Handle<JSObject*> aEvaluationGlobal,
                          JS::CompileOptions& aCompileOptions)
{
  EvaluateOptions options(aCx);
  aCompileOptions.setNoScriptRval(true);
  JS::RootedValue unused(aCx);
  return EvaluateString(aCx, aScript, aEvaluationGlobal, aCompileOptions,
                        options, &unused);
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          JS::SourceBufferHolder& aSrcBuf,
                          JS::Handle<JSObject*> aEvaluationGlobal,
                          JS::CompileOptions& aCompileOptions,
                          void **aOffThreadToken)
{
  EvaluateOptions options(aCx);
  aCompileOptions.setNoScriptRval(true);
  JS::RootedValue unused(aCx);
  return EvaluateString(aCx, aSrcBuf, aEvaluationGlobal, aCompileOptions,
                        options, &unused, aOffThreadToken);
}

/* static */
bool
nsJSUtils::GetScopeChainForElement(JSContext* aCx,
                                   mozilla::dom::Element* aElement,
                                   JS::AutoObjectVector& aScopeChain)
{
  for (nsINode* cur = aElement; cur; cur = cur->GetScopeChainParent()) {
    JS::RootedValue val(aCx);
    if (!WrapNewBindingObject(aCx, cur, &val)) {
      return false;
    }

    if (!aScopeChain.append(&val.toObject())) {
      return false;
    }
  }

  return true;
}


//
// nsDOMJSUtils.h
//

JSObject* GetDefaultScopeFromJSContext(JSContext *cx)
{
  // DOM JSContexts don't store their default compartment object on
  // the cx, so in those cases we need to fetch it via the scx
  // instead.
  nsIScriptContext *scx = GetScriptContextFromJSContext(cx);
  return  scx ? scx->GetWindowProxy() : nullptr;
}
