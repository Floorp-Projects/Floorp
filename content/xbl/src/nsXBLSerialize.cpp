/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXBLSerialize.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "jsdbgapi.h"

using namespace mozilla;

nsresult
XBL_SerializeFunction(nsIScriptContext* aContext,
                      nsIObjectOutputStream* aStream,
                      JSObject* aFunctionObject)
{
  AutoPushJSContext cx(aContext->GetNativeContext());
  return nsContentUtils::XPConnect()->WriteFunction(aStream, cx, aFunctionObject);
}

nsresult
XBL_DeserializeFunction(nsIScriptContext* aContext,
                        nsIObjectInputStream* aStream,
                        JSObject** aFunctionObjectp)
{
  AutoPushJSContext cx(aContext->GetNativeContext());
  nsresult rv = nsContentUtils::XPConnect()->ReadFunction(aStream, cx, aFunctionObjectp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Mark the script as XBL.
  //
  // This might be more elegantly handled as a flag via the XPConnect serialization
  // code, but that would involve profile compat issues between different builds.
  // Given that we know this code is XBL, just flag it as such.
  JSAutoRequest ar(cx);
  JSFunction* fun = JS_ValueToFunction(cx, JS::ObjectValue(**aFunctionObjectp));
  NS_ENSURE_TRUE(fun, NS_ERROR_UNEXPECTED);
  JS_SetScriptUserBit(JS_GetFunctionScript(cx, fun), true);
  return NS_OK;
}
