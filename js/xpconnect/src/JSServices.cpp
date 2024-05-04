/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "StaticComponents.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ProfilerLabels.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefinePropertyById
#include "js/String.h"              // JS::LinearStringHasLatin1Chars
#include "nsJSUtils.h"

using namespace mozilla;
using namespace JS;

namespace xpc {

static bool Services_NewEnumerate(JSContext* cx, HandleObject obj,
                                  MutableHandleIdVector properties,
                                  bool enumerableOnly);
static bool Services_Resolve(JSContext* cx, HandleObject obj, HandleId id,
                             bool* resolvedp);
static bool Services_MayResolve(const JSAtomState& names, jsid id,
                                JSObject* maybeObj);

static const JSClassOps sServices_ClassOps = {
    nullptr,                // addProperty
    nullptr,                // delProperty
    nullptr,                // enumerate
    Services_NewEnumerate,  // newEnumerate
    Services_Resolve,       // resolve
    Services_MayResolve,    // mayResolve
    nullptr,                // finalize
    nullptr,                // call
    nullptr,                // construct
    nullptr,                // trace
};

static const JSClass sServices_Class = {"JSServices", 0, &sServices_ClassOps};

JSObject* NewJSServices(JSContext* cx) {
  return JS_NewObject(cx, &sServices_Class);
}

static bool Services_NewEnumerate(JSContext* cx, HandleObject obj,
                                  MutableHandleIdVector properties,
                                  bool enumerableOnly) {
  auto services = xpcom::StaticComponents::GetJSServices();

  if (!properties.reserve(services.Length())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  RootedId id(cx);
  RootedString name(cx);
  for (const auto& service : services) {
    name = JS_AtomizeString(cx, service.Name().get());
    if (!name || !JS_StringToId(cx, name, &id)) {
      return false;
    }
    properties.infallibleAppend(id);
  }

  return true;
}

static JSLinearString* GetNameIfLatin1(jsid id) {
  if (id.isString()) {
    JSLinearString* name = id.toLinearString();
    if (JS::LinearStringHasLatin1Chars(name)) {
      return name;
    }
  }
  return nullptr;
}

static bool GetServiceImpl(JSContext* cx, const xpcom::JSServiceEntry& service,
                           JS::MutableHandleObject aObj, ErrorResult& aRv) {
  nsresult rv;
  nsCOMPtr<nsISupports> inst = service.Module().GetService(&rv);
  if (!inst) {
    aRv.Throw(rv);
    return false;
  }

  auto ifaces = service.Interfaces();

  if (ifaces.Length() == 0) {
    // If we weren't given any interfaces, we're expecting either a WebIDL
    // object or a wrapped JS object. In the former case, the object will handle
    // its own wrapping, and there's nothing to do. In the latter case, we want
    // to unwrap the underlying JS object.
    if (nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(inst)) {
      aObj.set(wrappedJS->GetJSObject());
      return !!aObj;
    }
  }

  JS::RootedValue val(cx);

  const nsIID* iid = ifaces.Length() ? ifaces[0] : nullptr;
  xpcObjectHelper helper(inst);
  if (!XPCConvert::NativeInterface2JSObject(cx, &val, helper, iid,
                                            /* allowNativeWrapper */ true,
                                            &rv)) {
    aRv.Throw(rv);
    return false;
  }

  if (ifaces.Length() > 1) {
    auto* wn = XPCWrappedNative::Get(&val.toObject());
    for (const nsIID* iid : Span(ifaces).From(1)) {
      // Ignore any supplemental interfaces that aren't implemented. Tests do
      // weird things with some services, and JS can generally handle the
      // interfaces being absent.
      Unused << wn->FindTearOff(cx, *iid);
    }
  }

  aObj.set(&val.toObject());
  return true;
}

static JSObject* GetService(JSContext* cx, const xpcom::JSServiceEntry& service,
                            ErrorResult& aRv) {
  JS::RootedObject obj(cx);
  if (!GetServiceImpl(cx, service, &obj, aRv)) {
    return nullptr;
  }
  return obj;
}

static bool Services_Resolve(JSContext* cx, HandleObject obj, HandleId id,
                             bool* resolvedp) {
  *resolvedp = false;
  JSLinearString* name = GetNameIfLatin1(id);
  if (!name) {
    return true;
  }

  nsAutoJSLinearCString nameStr(name);
  if (const auto* service = xpcom::JSServiceEntry::Lookup(nameStr)) {
    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_NONSENSITIVE("Services_Resolve",
                                                       OTHER, service->Name());
    *resolvedp = true;

    ErrorResult rv;
    JS::RootedValue val(cx);

    val.setObjectOrNull(GetService(cx, *service, rv));
    if (rv.MaybeSetPendingException(cx)) {
      return false;
    }

    return JS_DefinePropertyById(cx, obj, id, val, JSPROP_ENUMERATE);
  }
  return true;
}

static bool Services_MayResolve(const JSAtomState& names, jsid id,
                                JSObject* maybeObj) {
  if (JSLinearString* name = GetNameIfLatin1(id)) {
    nsAutoJSLinearCString nameStr(name);
    return xpcom::JSServiceEntry::Lookup(nameStr);
  }
  return false;
}

}  // namespace xpc
