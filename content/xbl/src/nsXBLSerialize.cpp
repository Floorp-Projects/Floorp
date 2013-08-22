/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXBLSerialize.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "jsdbgapi.h"

using namespace mozilla;

nsresult
XBL_SerializeFunction(nsIObjectOutputStream* aStream,
                      JS::Handle<JSObject*> aFunction)
{
  AssertInCompilationScope();
  AutoJSContext cx;
  MOZ_ASSERT(js::GetContextCompartment(cx) == js::GetObjectCompartment(aFunction));
  return nsContentUtils::XPConnect()->WriteFunction(aStream, cx, aFunction);
}

nsresult
XBL_DeserializeFunction(nsIObjectInputStream* aStream,
                        JS::MutableHandle<JSObject*> aFunctionObjectp)
{
  AssertInCompilationScope();
  AutoJSContext cx;
  return nsContentUtils::XPConnect()->ReadFunction(aStream, cx,
                                                   aFunctionObjectp.address());
}
