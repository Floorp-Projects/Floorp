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
                      JS::Handle<JSObject*> aFunction)
{
  AutoPushJSContext cx(aContext->GetNativeContext());
  return nsContentUtils::XPConnect()->WriteFunction(aStream, cx, aFunction);
}

nsresult
XBL_DeserializeFunction(nsIScriptContext* aContext,
                        nsIObjectInputStream* aStream,
                        JS::MutableHandle<JSObject*> aFunctionObjectp)
{
  AutoPushJSContext cx(aContext->GetNativeContext());
  return nsContentUtils::XPConnect()->ReadFunction(aStream, cx,
                                                   aFunctionObjectp.address());
}
