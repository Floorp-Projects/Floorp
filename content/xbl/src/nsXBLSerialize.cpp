/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXBLSerialize.h"
#include "nsDOMScriptObjectHolder.h"
#include "nsContentUtils.h"

nsresult
XBL_SerializeFunction(nsIScriptContext* aContext,
                      nsIObjectOutputStream* aStream,
                      JSObject* aFunctionObject)
{
  JSContext* cx = aContext->GetNativeContext();
  return nsContentUtils::XPConnect()->WriteFunction(aStream, cx, aFunctionObject);
}

nsresult
XBL_DeserializeFunction(nsIScriptContext* aContext,
                        nsIObjectInputStream* aStream,
                        JSObject** aFunctionObjectp)
{
  JSContext* cx = aContext->GetNativeContext();
  return nsContentUtils::XPConnect()->ReadFunction(aStream, cx, aFunctionObjectp);
}
