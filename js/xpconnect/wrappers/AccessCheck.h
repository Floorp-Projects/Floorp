/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AccessCheck_h__
#define __AccessCheck_h__

#include "js/Id.h"
#include "js/Wrapper.h"
#include "nsString.h"

class nsIPrincipal;

namespace xpc {

class AccessCheck {
 public:
  static bool subsumes(JSObject* a, JSObject* b);
  static bool wrapperSubsumes(JSObject* wrapper);
  static bool subsumesConsideringDomain(JS::Compartment* a, JS::Compartment* b);
  static bool subsumesConsideringDomainIgnoringFPD(JS::Compartment* a,
                                                   JS::Compartment* b);
  static bool isChrome(JS::Compartment* compartment);
  static bool isChrome(JSObject* obj);
  static bool checkPassToPrivilegedCode(JSContext* cx, JS::HandleObject wrapper,
                                        JS::HandleValue value);
  static bool checkPassToPrivilegedCode(JSContext* cx, JS::HandleObject wrapper,
                                        const JS::CallArgs& args);
  // Called to report the correct sort of exception when our policy denies and
  // should throw.  The accessType argument should be one of "access",
  // "define", "delete", depending on which operation is being denied.
  static void reportCrossOriginDenial(JSContext* cx, JS::HandleId id,
                                      const nsACString& accessType);
};

enum CrossOriginObjectType {
  CrossOriginWindow,
  CrossOriginLocation,
  CrossOriginOpaque
};
CrossOriginObjectType IdentifyCrossOriginObject(JSObject* obj);

struct Policy {
  static bool checkCall(JSContext* cx, JS::HandleObject wrapper,
                        const JS::CallArgs& args) {
    MOZ_CRASH("As a rule, filtering wrappers are non-callable");
  }
};

// This policy allows no interaction with the underlying callable. Everything
// throws.
struct Opaque : public Policy {
  static bool check(JSContext* cx, JSObject* wrapper, jsid id,
                    js::Wrapper::Action act) {
    return false;
  }
  static bool deny(JSContext* cx, js::Wrapper::Action act, JS::HandleId id,
                   bool mayThrow) {
    return false;
  }
  static bool allowNativeCall(JSContext* cx, JS::IsAcceptableThis test,
                              JS::NativeImpl impl) {
    return false;
  }
};

// Like the above, but allows CALL.
struct OpaqueWithCall : public Policy {
  static bool check(JSContext* cx, JSObject* wrapper, jsid id,
                    js::Wrapper::Action act) {
    return act == js::Wrapper::CALL;
  }
  static bool deny(JSContext* cx, js::Wrapper::Action act, JS::HandleId id,
                   bool mayThrow) {
    return false;
  }
  static bool allowNativeCall(JSContext* cx, JS::IsAcceptableThis test,
                              JS::NativeImpl impl) {
    return false;
  }
  static bool checkCall(JSContext* cx, JS::HandleObject wrapper,
                        const JS::CallArgs& args) {
    return AccessCheck::checkPassToPrivilegedCode(cx, wrapper, args);
  }
};

// This class used to support permitting access to properties if they
// appeared in an access list on the object, but now it acts like an
// Opaque wrapper, with the exception that it fails silently for GET,
// ENUMERATE, and GET_PROPERTY_DESCRIPTOR. This is done for backwards
// compatibility. See bug 1397513.
struct OpaqueWithSilentFailing : public Policy {
  static bool check(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                    js::Wrapper::Action act) {
    return false;
  }

  static bool deny(JSContext* cx, js::Wrapper::Action act, JS::HandleId id,
                   bool mayThrow);
  static bool allowNativeCall(JSContext* cx, JS::IsAcceptableThis test,
                              JS::NativeImpl impl) {
    return false;
  }
};

}  // namespace xpc

#endif /* __AccessCheck_h__ */
