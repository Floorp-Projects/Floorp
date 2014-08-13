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
#include "js/OldDebugAPI.h"
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

nsIScriptGlobalObject *
nsJSUtils::GetDynamicScriptGlobal(JSContext* aContext)
{
  nsIScriptContext *scriptCX = GetDynamicScriptContext(aContext);
  if (!scriptCX)
    return nullptr;
  return scriptCX->GetGlobalObject();
}

nsIScriptContext *
nsJSUtils::GetDynamicScriptContext(JSContext *aContext)
{
  return GetScriptContextFromJSContext(aContext);
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
      scope = scx ? scx->GetWindowProxy()
                  : js::DefaultObjectForContextOrNull(aContext);
      if (!scope) {
        // The SafeJSContext has no default object associated with it.
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(aContext == nsContentUtils::GetSafeJSContext());
        scope = xpc::GetSafeJSContextGlobal();
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
nsJSUtils::CompileFunction(JSContext* aCx,
                           JS::Handle<JSObject*> aTarget,
                           JS::CompileOptions& aOptions,
                           const nsACString& aName,
                           uint32_t aArgCount,
                           const char** aArgArray,
                           const nsAString& aBody,
                           JSObject** aFunctionObject)
{
  MOZ_ASSERT(js::GetEnterCompartmentDepth(aCx) > 0);
  MOZ_ASSERT_IF(aTarget, js::IsObjectInContextCompartment(aTarget, aCx));
  MOZ_ASSERT_IF(aOptions.versionSet, aOptions.version != JSVERSION_UNKNOWN);
  mozilla::DebugOnly<nsIScriptContext*> ctx = GetScriptContextFromJSContext(aCx);
  MOZ_ASSERT_IF(ctx, ctx->IsContextInitialized());

  // Do the junk Gecko is supposed to do before calling into JSAPI.
  if (aTarget) {
    JS::ExposeObjectToActiveJS(aTarget);
  }

  // Compile.
  JS::Rooted<JSFunction*> fun(aCx);
  if (!JS::CompileFunction(aCx, aTarget, aOptions,
                           PromiseFlatCString(aName).get(),
                           aArgCount, aArgArray,
                           PromiseFlatString(aBody).get(),
                           aBody.Length(), &fun))
  {
    ReportPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  *aFunctionObject = JS_GetFunctionObject(fun);
  return NS_OK;
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          const nsAString& aScript,
                          JS::Handle<JSObject*> aScopeObject,
                          JS::CompileOptions& aCompileOptions,
                          const EvaluateOptions& aEvaluateOptions,
                          JS::MutableHandle<JS::Value> aRetValue,
                          void **aOffThreadToken)
{
  const nsPromiseFlatString& flatScript = PromiseFlatString(aScript);
  JS::SourceBufferHolder srcBuf(flatScript.get(), aScript.Length(),
                                JS::SourceBufferHolder::NoOwnership);
  return EvaluateString(aCx, srcBuf, aScopeObject, aCompileOptions,
                        aEvaluateOptions, aRetValue, aOffThreadToken);
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          JS::SourceBufferHolder& aSrcBuf,
                          JS::Handle<JSObject*> aScopeObject,
                          JS::CompileOptions& aCompileOptions,
                          const EvaluateOptions& aEvaluateOptions,
                          JS::MutableHandle<JS::Value> aRetValue,
                          void **aOffThreadToken)
{
  PROFILER_LABEL("nsJSUtils", "EvaluateString",
    js::ProfileEntry::Category::JS);

  MOZ_ASSERT_IF(aCompileOptions.versionSet,
                aCompileOptions.version != JSVERSION_UNKNOWN);
  MOZ_ASSERT_IF(aEvaluateOptions.coerceToString, aEvaluateOptions.needResult);
  MOZ_ASSERT_IF(!aEvaluateOptions.reportUncaught, aEvaluateOptions.needResult);
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());
  MOZ_ASSERT(aSrcBuf.get());

  // Unfortunately, the JS engine actually compiles scripts with a return value
  // in a different, less efficient way.  Furthermore, it can't JIT them in many
  // cases.  So we need to be explicitly told whether the caller cares about the
  // return value.  Callers can do this by calling the other overload of
  // EvaluateString() which calls this function with aEvaluateOptions.needResult
  // set to false.
  aRetValue.setUndefined();

  JS::ExposeObjectToActiveJS(aScopeObject);
  nsAutoMicroTask mt;
  nsresult rv = NS_OK;

  bool ok = false;
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(ssm->ScriptAllowed(js::GetGlobalForObjectCrossCompartment(aScopeObject)), NS_OK);

  mozilla::Maybe<AutoDontReportUncaught> dontReport;
  if (!aEvaluateOptions.reportUncaught) {
    // We need to prevent AutoLastFrameCheck from reporting and clearing
    // any pending exceptions.
    dontReport.emplace(aCx);
  }

  // Scope the JSAutoCompartment so that we can later wrap the return value
  // into the caller's cx.
  {
    JSAutoCompartment ac(aCx, aScopeObject);

    JS::Rooted<JSObject*> rootedScope(aCx, aScopeObject);
    if (aOffThreadToken) {
      JS::Rooted<JSScript*>
        script(aCx, JS::FinishOffThreadScript(aCx, JS_GetRuntime(aCx), *aOffThreadToken));
      *aOffThreadToken = nullptr; // Mark the token as having been finished.
      if (script) {
        if (aEvaluateOptions.needResult) {
          ok = JS_ExecuteScript(aCx, rootedScope, script, aRetValue);
        } else {
          ok = JS_ExecuteScript(aCx, rootedScope, script);
        }
      } else {
        ok = false;
      }
    } else {
      if (aEvaluateOptions.needResult) {
        ok = JS::Evaluate(aCx, rootedScope, aCompileOptions,
                          aSrcBuf, aRetValue);
      } else {
        ok = JS::Evaluate(aCx, rootedScope, aCompileOptions,
                          aSrcBuf);
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
      if (aEvaluateOptions.needResult) {
        aRetValue.setUndefined();
      }
    } else {
      rv = JS_IsExceptionPending(aCx) ? NS_ERROR_FAILURE
                                      : NS_ERROR_OUT_OF_MEMORY;
      JS::Rooted<JS::Value> exn(aCx);
      JS_GetPendingException(aCx, &exn);
      if (aEvaluateOptions.needResult) {
        aRetValue.set(exn);
      }
      JS_ClearPendingException(aCx);
    }
  }

  // Wrap the return value into whatever compartment aCx was in.
  if (aEvaluateOptions.needResult) {
    JS::Rooted<JS::Value> v(aCx, aRetValue);
    if (!JS_WrapValue(aCx, &v)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aRetValue.set(v);
  }
  return rv;
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          const nsAString& aScript,
                          JS::Handle<JSObject*> aScopeObject,
                          JS::CompileOptions& aCompileOptions,
                          void **aOffThreadToken)
{
  EvaluateOptions options;
  options.setNeedResult(false);
  JS::RootedValue unused(aCx);
  return EvaluateString(aCx, aScript, aScopeObject, aCompileOptions,
                        options, &unused, aOffThreadToken);
}

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          JS::SourceBufferHolder& aSrcBuf,
                          JS::Handle<JSObject*> aScopeObject,
                          JS::CompileOptions& aCompileOptions,
                          void **aOffThreadToken)
{
  EvaluateOptions options;
  options.setNeedResult(false);
  JS::RootedValue unused(aCx);
  return EvaluateString(aCx, aSrcBuf, aScopeObject, aCompileOptions,
                        options, &unused, aOffThreadToken);
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
  if (scx) {
    return scx->GetWindowProxy();
  }
  return js::DefaultObjectForContextOrNull(cx);
}
