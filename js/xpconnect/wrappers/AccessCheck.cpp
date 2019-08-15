/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"

#include "nsJSPrincipals.h"
#include "nsGlobalWindow.h"

#include "XPCWrapper.h"
#include "XrayWrapper.h"
#include "FilteringWrapper.h"

#include "jsfriendapi.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/LocationBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
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
  return CompartmentOriginInfo::Subsumes(js::GetObjectCompartment(a),
                                         js::GetObjectCompartment(b));
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
  return CompartmentOriginInfo::Subsumes(js::GetObjectCompartment(wrapper),
                                         js::GetObjectCompartment(wrapped));
}

bool AccessCheck::isChrome(JS::Compartment* compartment) {
  return js::IsSystemCompartment(compartment);
}

bool AccessCheck::isChrome(JS::Realm* realm) {
  return isChrome(JS::GetCompartmentForRealm(realm));
}

bool AccessCheck::isChrome(JSObject* obj) {
  return isChrome(js::GetObjectCompartment(obj));
}

bool IsCrossOriginAccessibleObject(JSObject* obj) {
  obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  const JSClass* clasp = js::GetObjectClass(obj);

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

  // CPOWs use COWs (in the unprivileged junk scope) for all child->parent
  // references. Without this test, the child process wouldn't be able to
  // pass any objects at all to CPOWs.
  if (mozilla::jsipc::IsWrappedCPOW(obj) &&
      js::GetObjectCompartment(wrapper) ==
          js::GetObjectCompartment(xpc::UnprivilegedJunkScope()) &&
      XRE_IsParentProcess()) {
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
  if (JSID_IS_VOID(id)) {
    message = NS_LITERAL_CSTRING("Permission denied to access object");
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
    message = NS_LITERAL_CSTRING("Permission denied to ") + accessType +
              NS_LITERAL_CSTRING(" property ") +
              NS_ConvertUTF16toUTF8(propName) +
              NS_LITERAL_CSTRING(" on cross-origin object");
  }
  ErrorResult rv;
  rv.ThrowDOMException(NS_ERROR_DOM_SECURITY_ERR, message);
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
