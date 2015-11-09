/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSDebugger.h"
#include "nsIXPConnect.h"
#include "nsThreadUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "jswrapper.h"
#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsMemory.h"

#define JSDEBUGGER_CONTRACTID \
  "@mozilla.org/jsdebugger;1"

#define JSDEBUGGER_CID \
{ 0x0365cbd5, 0xd46e, 0x4e94, { 0xa3, 0x9f, 0x83, 0xb6, 0x3c, 0xd1, 0xa9, 0x63 } }

namespace mozilla {
namespace jsdebugger {

NS_GENERIC_FACTORY_CONSTRUCTOR(JSDebugger)

NS_IMPL_ISUPPORTS(JSDebugger, IJSDebugger)

JSDebugger::JSDebugger()
{
}

JSDebugger::~JSDebugger()
{
}

NS_IMETHODIMP
JSDebugger::AddClass(JS::Handle<JS::Value> global, JSContext* cx)
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);

  if (!global.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::RootedObject obj(cx, &global.toObject());
  obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  if (!obj) {
    return NS_ERROR_FAILURE;
  }

  JSAutoCompartment ac(cx, obj);
  if (JS_GetGlobalForObject(cx, obj) != obj) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!JS_DefineDebuggerObject(cx, obj)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace jsdebugger
} // namespace mozilla

NS_DEFINE_NAMED_CID(JSDEBUGGER_CID);

static const mozilla::Module::CIDEntry kJSDebuggerCIDs[] = {
  { &kJSDEBUGGER_CID, false, nullptr, mozilla::jsdebugger::JSDebuggerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kJSDebuggerContracts[] = {
  { JSDEBUGGER_CONTRACTID, &kJSDEBUGGER_CID },
  { nullptr }
};

static const mozilla::Module kJSDebuggerModule = {
  mozilla::Module::kVersion,
  kJSDebuggerCIDs,
  kJSDebuggerContracts
};

NSMODULE_DEFN(jsdebugger) = &kJSDebuggerModule;
