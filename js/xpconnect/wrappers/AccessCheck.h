/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AccessCheck_h__
#define __AccessCheck_h__

#include "jswrapper.h"
#include "js/Id.h"

class nsIPrincipal;

namespace xpc {

class AccessCheck {
  public:
    static bool subsumes(JSCompartment *a, JSCompartment *b);
    static bool subsumes(JSObject *a, JSObject *b);
    static bool wrapperSubsumes(JSObject *wrapper);
    static bool subsumesConsideringDomain(JSCompartment *a, JSCompartment *b);
    static bool isChrome(JSCompartment *compartment);
    static bool isChrome(JSObject *obj);
    static nsIPrincipal *getPrincipal(JSCompartment *compartment);
    static bool isCrossOriginAccessPermitted(JSContext *cx, JS::HandleObject obj,
                                             JS::HandleId id, js::Wrapper::Action act);
    static bool checkPassToPrivilegedCode(JSContext *cx, JS::HandleObject wrapper,
                                          JS::HandleValue value);
    static bool checkPassToPrivilegedCode(JSContext *cx, JS::HandleObject wrapper,
                                          const JS::CallArgs &args);
};

enum CrossOriginObjectType {
    CrossOriginWindow,
    CrossOriginLocation,
    CrossOriginOpaque
};
CrossOriginObjectType IdentifyCrossOriginObject(JSObject *obj);

struct Policy {
    static const bool AllowGetPrototypeOf = false;

    static bool checkCall(JSContext *cx, JS::HandleObject wrapper, const JS::CallArgs &args) {
        MOZ_CRASH("As a rule, filtering wrappers are non-callable");
    }
};

// This policy allows no interaction with the underlying callable. Everything throws.
struct Opaque : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act) {
        return false;
    }
    static bool deny(js::Wrapper::Action act, JS::HandleId id) {
        return false;
    }
    static bool allowNativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl) {
        return false;
    }
};

// Like the above, but allows CALL.
struct OpaqueWithCall : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act) {
        return act == js::Wrapper::CALL;
    }
    static bool deny(js::Wrapper::Action act, JS::HandleId id) {
        return false;
    }
    static bool allowNativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl) {
        return false;
    }
    static bool checkCall(JSContext *cx, JS::HandleObject wrapper, const JS::CallArgs &args) {
        return AccessCheck::checkPassToPrivilegedCode(cx, wrapper, args);
    }
};

// This policy only permits access to properties that are safe to be used
// across origins.
struct CrossOriginAccessiblePropertiesOnly : public Policy {
    static bool check(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id, js::Wrapper::Action act) {
        return AccessCheck::isCrossOriginAccessPermitted(cx, wrapper, id, act);
    }
    static bool deny(js::Wrapper::Action act, JS::HandleId id) {
        // Silently fail for enumerate-like operations.
        if (act == js::Wrapper::ENUMERATE)
            return true;
        return false;
    }
    static bool allowNativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl) {
        return false;
    }
};

// This policy only permits access to properties if they appear in the
// objects exposed properties list.
struct ExposedPropertiesOnly : public Policy {

    // COWs are the only type of filtering wrapper that allow access to the
    // prototype, because the standard prototypes are remapped into the
    // wrapper's compartment.
    static const bool AllowGetPrototypeOf = true;

    static bool check(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id, js::Wrapper::Action act);

    static bool deny(js::Wrapper::Action act, JS::HandleId id);
    static bool allowNativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl) {
        return false;
    }
};

}

#endif /* __AccessCheck_h__ */
