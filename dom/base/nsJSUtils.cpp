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
#include "prprf.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsIScriptSecurityManager.h"
#include "nsPIDOMWindow.h"

#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext

#include "mozilla/dom/BindingUtils.h"

JSBool
nsJSUtils::GetCallingLocation(JSContext* aContext, const char* *aFilename,
                              PRUint32* aLineno)
{
  JSScript* script = nsnull;
  unsigned lineno = 0;

  if (!JS_DescribeScriptedCaller(aContext, &script, &lineno)) {
    return JS_FALSE;
  }

  *aFilename = ::JS_GetScriptFilename(aContext, script);
  *aLineno = lineno;

  return JS_TRUE;
}

nsIScriptGlobalObject *
nsJSUtils::GetStaticScriptGlobal(JSContext* aContext, JSObject* aObj)
{
  JSClass* clazz;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return nsnull;

  glob = JS_GetGlobalForObject(aContext, glob);
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
    return nsnull;
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
    NS_ENSURE_TRUE(wrapper, nsnull);
    sgo = do_QueryWrappedNative(wrapper);
  }

  // We're returning a pointer to something that's about to be
  // released, but that's ok here.
  return sgo;
}

nsIScriptContext *
nsJSUtils::GetStaticScriptContext(JSContext* aContext, JSObject* aObj)
{
  nsIScriptGlobalObject *nativeGlobal = GetStaticScriptGlobal(aContext, aObj);
  if (!nativeGlobal)
    return nsnull;

  return nativeGlobal->GetScriptContext();
}

nsIScriptGlobalObject *
nsJSUtils::GetDynamicScriptGlobal(JSContext* aContext)
{
  nsIScriptContext *scriptCX = GetDynamicScriptContext(aContext);
  if (!scriptCX)
    return nsnull;
  return scriptCX->GetGlobalObject();
}

nsIScriptContext *
nsJSUtils::GetDynamicScriptContext(JSContext *aContext)
{
  return GetScriptContextFromJSContext(aContext);
}

PRUint64
nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(JSContext *aContext)
{
  if (!aContext)
    return 0;

  PRUint64 innerWindowID = 0;

  JSObject *jsGlobal = JS_GetGlobalForScopeChain(aContext);
  if (jsGlobal) {
    nsIScriptGlobalObject *scriptGlobal = GetStaticScriptGlobal(aContext,
                                                                jsGlobal);
    if (scriptGlobal) {
      nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(scriptGlobal);
      if (win)
        innerWindowID = win->WindowID();
    }
  }

  return innerWindowID;
}

