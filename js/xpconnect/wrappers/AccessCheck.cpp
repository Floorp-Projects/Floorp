/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"

#include "nsJSPrincipals.h"

#include "XPCWrapper.h"
#include "XrayWrapper.h"
#include "FilteringWrapper.h"

#include "jsfriendapi.h"
#include "js/Object.h"  // JS::GetClass, JS::GetCompartment
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"

using namespace mozilla;
using namespace JS;
using namespace js;

namespace xpc {

BasePrincipal* GetRealmPrincipal(JS::Realm* realm) {
  return BasePrincipal::Cast(
      nsJSPrincipals::get(JS::GetRealmPrincipals(realm)));
}

nsIPrincipal* GetObjectPrincipal(JSObject* obj) {
  return GetRealmPrincipal(js::GetNonCCWObjectRealm(obj));
}

bool AccessCheck::subsumes(JSObject* a, JSObject* b) {
  return CompartmentOriginInfo::Subsumes(JS::GetCompartment(a),
                                         JS::GetCompartment(b));
}

// Same as above, but considering document.domain.
bool AccessCheck::subsumesConsideringDomain(JS::Realm* a, JS::Realm* b) {
  MOZ_ASSERT(OriginAttributes::IsRestrictOpenerAccessForFPI());
  BasePrincipal* aprin = GetRealmPrincipal(a);
  BasePrincipal* bprin = GetRealmPrincipal(b);
  return aprin->FastSubsumesConsideringDomain(bprin);
}

bool AccessCheck::subsumesConsideringDomainIgnoringFPD(JS::Realm* a,
                                                       JS::Realm* b) {
  MOZ_ASSERT(!OriginAttributes::IsRestrictOpenerAccessForFPI());
  BasePrincipal* aprin = GetRealmPrincipal(a);
  BasePrincipal* bprin = GetRealmPrincipal(b);
  return aprin->FastSubsumesConsideringDomainIgnoringFPD(bprin);
}

// Does the compartment of the wrapper subsumes the compartment of the wrappee?
bool AccessCheck::wrapperSubsumes(JSObject* wrapper) {
  MOZ_ASSERT(js::IsWrapper(wrapper));
  JSObject* wrapped = js::UncheckedUnwrap(wrapper);
  return CompartmentOriginInfo::Subsumes(JS::GetCompartment(wrapper),
                                         JS::GetCompartment(wrapped));
}

bool AccessCheck::isChrome(JS::Compartment* compartment) {
  return js::IsSystemCompartment(compartment);
}

bool AccessCheck::isChrome(JS::Realm* realm) {
  return isChrome(JS::GetCompartmentForRealm(realm));
}

bool AccessCheck::isChrome(JSObject* obj) {
  return isChrome(JS::GetCompartment(obj));
}

bool IsCrossOriginAccessibleObject(JSObject* obj) {
  obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  const JSClass* clasp = JS::GetClass(obj);

  return (clasp->name[0] == 'L' && !strcmp(clasp->name, "Location")) ||
         (clasp->name[0] == 'W' && !strcmp(clasp->name, "Window"));
}

bool AccessCheck::checkPassToPrivilegedCode(JSContext* cx, HandleObject wrapper,
                                            HandleValue v) {
  // Primitives are fine.
  if (!v.isObject()) {
    return true;
  }
  RootedObject obj(cx, &v.toObject());

  // Non-wrappers are fine.
  if (!js::IsWrapper(obj)) {
    return true;
  }

  // Same-origin wrappers are fine.
  if (AccessCheck::wrapperSubsumes(obj)) {
    return true;
  }

  // Badness.
  JS_ReportErrorASCII(cx,
                      "Permission denied to pass object to privileged code");
  return false;
}

bool AccessCheck::checkPassToPrivilegedCode(JSContext* cx, HandleObject wrapper,
                                            const CallArgs& args) {
  if (!checkPassToPrivilegedCode(cx, wrapper, args.thisv())) {
    return false;
  }
  for (size_t i = 0; i < args.length(); ++i) {
    if (!checkPassToPrivilegedCode(cx, wrapper, args[i])) {
      return false;
    }
  }
  return true;
}

void AccessCheck::reportCrossOriginDenial(JSContext* cx, JS::HandleId id,
                                          const nsACString& accessType) {
  // This function exists because we want to report DOM SecurityErrors, not JS
  // Errors, when denying access on cross-origin DOM objects.  It's
  // conceptually pretty similar to
  // AutoEnterPolicy::reportErrorIfExceptionIsNotPending.
  if (JS_IsExceptionPending(cx)) {
    return;
  }

  nsAutoCString message;
  if (id.isVoid()) {
    message = "Permission denied to access object"_ns;
  } else {
    // We want to use JS_ValueToSource here, because that most closely
    // matches what AutoEnterPolicy::reportErrorIfExceptionIsNotPending
    // does.
    JS::RootedValue idVal(cx, js::IdToValue(id));
    nsAutoJSString propName;
    JS::RootedString idStr(cx, JS_ValueToSource(cx, idVal));
    if (!idStr || !propName.init(cx, idStr)) {
      return;
    }
    message = "Permission denied to "_ns + accessType + " property "_ns +
              NS_ConvertUTF16toUTF8(propName) + " on cross-origin object"_ns;
  }
  ErrorResult rv;
  rv.ThrowSecurityError(message);
  MOZ_ALWAYS_TRUE(rv.MaybeSetPendingException(cx));
}

bool OpaqueWithSilentFailing::deny(JSContext* cx, js::Wrapper::Action act,
                                   HandleId id, bool mayThrow) {
  // Fail silently for GET, ENUMERATE, and GET_PROPERTY_DESCRIPTOR.
  if (act == js::Wrapper::GET || act == js::Wrapper::ENUMERATE ||
      act == js::Wrapper::GET_PROPERTY_DESCRIPTOR) {
    // Note that ReportWrapperDenial doesn't do any _exception_ reporting,
    // so we want to do this regardless of the value of mayThrow.
    return ReportWrapperDenial(cx, id, WrapperDenialForCOW,
                               "Access to privileged JS object not permitted");
  }

  return false;
}

}  // namespace xpc
