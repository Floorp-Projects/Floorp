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
#include "jsdbgapi.h"
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

bool
nsJSUtils::GetCallingLocation(JSContext* aContext, const char* *aFilename,
                              uint32_t* aLineno)
{
  JSScript* script = nullptr;
  unsigned lineno = 0;

  if (!JS_DescribeScriptedCaller(aContext, &script, &lineno)) {
    return false;
  }

  *aFilename = ::JS_GetScriptFilename(aContext, script);
  *aLineno = lineno;

  return true;
}

nsIScriptGlobalObject *
nsJSUtils::GetStaticScriptGlobal(JSObject* aObj)
{
  JSClass* clazz;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return nullptr;

  glob = js::GetGlobalForObjectCrossCompartment(glob);
  NS_ABORT_IF_FALSE(glob, "Infallible returns null");

  clazz = JS_GetClass(glob);

  // Whenever we end up with globals that are JSCLASS_IS_DOMJSCLASS
  // and have an nsISupports DOM object, we will need to modify this
  // check here.
  MOZ_ASSERT(!(clazz->flags & JSCLASS_IS_DOMJSCLASS));
  nsISupports* supports;
  if (!(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*)::JS_GetPrivate(glob))) {
    return nullptr;
  }

  // We might either have a window directly (e.g. if the global is a
  // sandbox whose script object principal pointer is a window), or an
  // XPCWrappedNative for a window.  We could also have other
  // sandbox-related script object principals, but we can't do much
  // about those short of trying to walk the proto chain of |glob|
  // looking for a window or something.
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(supports));
  if (!sgo) {
    nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(supports));
    if (!wrapper) {
      return nullptr;
    }
    sgo = do_QueryWrappedNative(wrapper);
  }

  // We're returning a pointer to something that's about to be
  // released, but that's ok here.
  return sgo;
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
      nsIScriptContext* scx = GetScriptContextFromJSContext(aContext);
      JS::Rooted<JSObject*> scope(aContext);
      scope = scx ? scx->GetWindowProxy()
                  : js::DefaultObjectForContextOrNull(aContext);
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
                           JS::HandleObject aTarget,
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

  // Since aTarget and aCx are same-compartment, there should be no distinction
  // between the object principal and the cx principal.
  // However, aTarget may be null in the wacky aShared case. So use the cx.
  JSPrincipals* p = JS_GetCompartmentPrincipals(js::GetContextCompartment(aCx));
  aOptions.setPrincipals(p);

  // Do the junk Gecko is supposed to do before calling into JSAPI.
  xpc_UnmarkGrayObject(aTarget);

  // Compile.
  JSFunction* fun = JS::CompileFunction(aCx, aTarget, aOptions,
                                        PromiseFlatCString(aName).get(),
                                        aArgCount, aArgArray,
                                        PromiseFlatString(aBody).get(),
                                        aBody.Length());
  if (!fun) {
    ReportPendingException(aCx);
    return NS_ERROR_FAILURE;
  }

  *aFunctionObject = JS_GetFunctionObject(fun);
  return NS_OK;
}

class MOZ_STACK_CLASS AutoDontReportUncaught {
  JSContext* mContext;
  bool mWasSet;

public:
  AutoDontReportUncaught(JSContext* aContext) : mContext(aContext) {
    MOZ_ASSERT(aContext);
    uint32_t oldOptions = JS_GetOptions(mContext);
    mWasSet = oldOptions & JSOPTION_DONT_REPORT_UNCAUGHT;
    if (!mWasSet) {
      JS_SetOptions(mContext, oldOptions | JSOPTION_DONT_REPORT_UNCAUGHT);
    }
  }
  ~AutoDontReportUncaught() {
    if (!mWasSet) {
      JS_SetOptions(mContext,
                    JS_GetOptions(mContext) & ~JSOPTION_DONT_REPORT_UNCAUGHT);
    }
  }
};

nsresult
nsJSUtils::EvaluateString(JSContext* aCx,
                          const nsAString& aScript,
                          JS::Handle<JSObject*> aScopeObject,
                          JS::CompileOptions& aCompileOptions,
                          EvaluateOptions& aEvaluateOptions,
                          JS::Value* aRetValue)
{
  PROFILER_LABEL("JS", "EvaluateString");
  MOZ_ASSERT_IF(aCompileOptions.versionSet,
                aCompileOptions.version != JSVERSION_UNKNOWN);
  MOZ_ASSERT_IF(aEvaluateOptions.coerceToString, aRetValue);
  MOZ_ASSERT_IF(!aEvaluateOptions.reportUncaught, aRetValue);
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());

  // Unfortunately, the JS engine actually compiles scripts with a return value
  // in a different, less efficient way.  Furthermore, it can't JIT them in many
  // cases.  So we need to be explicitly told whether the caller cares about the
  // return value.  Callers use null to indicate they don't care.
  if (aRetValue) {
    *aRetValue = JSVAL_VOID;
  }

  xpc_UnmarkGrayObject(aScopeObject);
  nsAutoMicroTask mt;

  JSPrincipals* p = JS_GetCompartmentPrincipals(js::GetObjectCompartment(aScopeObject));
  aCompileOptions.setPrincipals(p);

  bool ok = false;
  nsresult rv = nsContentUtils::GetSecurityManager()->
                  CanExecuteScripts(aCx, nsJSPrincipals::get(p), &ok);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(ok, NS_OK);

  mozilla::Maybe<AutoDontReportUncaught> dontReport;
  if (!aEvaluateOptions.reportUncaught) {
    // We need to prevent AutoLastFrameCheck from reporting and clearing
    // any pending exceptions.
    dontReport.construct(aCx);
  }

  // Scope the JSAutoCompartment so that we can later wrap the return value
  // into the caller's cx.
  {
    JSAutoCompartment ac(aCx, aScopeObject);

    JS::RootedObject rootedScope(aCx, aScopeObject);
    ok = JS::Evaluate(aCx, rootedScope, aCompileOptions,
                      PromiseFlatString(aScript).get(),
                      aScript.Length(), aRetValue);
    if (ok && aEvaluateOptions.coerceToString && !aRetValue->isUndefined()) {
      JSString* str = JS_ValueToString(aCx, *aRetValue);
      ok = !!str;
      *aRetValue = ok ? JS::StringValue(str) : JS::UndefinedValue();
    }
  }

  if (!ok) {
    if (aEvaluateOptions.reportUncaught) {
      ReportPendingException(aCx);
      if (aRetValue) {
        *aRetValue = JS::UndefinedValue();
      }
    } else {
      rv = JS_IsExceptionPending(aCx) ? NS_ERROR_FAILURE
                                      : NS_ERROR_OUT_OF_MEMORY;
      JS_GetPendingException(aCx, aRetValue);
      JS_ClearPendingException(aCx);
    }
  }

  // Wrap the return value into whatever compartment aCx was in.
  if (aRetValue && !JS_WrapValue(aCx, aRetValue))
    return NS_ERROR_OUT_OF_MEMORY;
  return rv;
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
