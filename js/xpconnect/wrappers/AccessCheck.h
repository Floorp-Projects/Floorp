/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
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
    static bool callerIsChrome();
    static nsIPrincipal *getPrincipal(JSCompartment *compartment);
    static bool isCrossOriginAccessPermitted(JSContext *cx, JSObject *obj, jsid id,
                                             js::Wrapper::Action act);
};

struct Policy {
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
};

// This policy is designed to protect privileged callers from untrusted non-
// Xrayable objects. Nothing is allowed, and nothing throws.
struct GentlyOpaque : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act) {
        return false;
    }
    static bool deny(js::Wrapper::Action act, JS::HandleId id) {
        return true;
    }
    static bool allowNativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl) {
        // We allow nativeCall here because the alternative is throwing (which
        // happens in SecurityWrapper::nativeCall), which we don't want. There's
        // unlikely to be too much harm to letting this through, because this
        // wrapper is only used to wrap less-privileged objects in more-privileged
        // scopes, so unwrapping here only drops privileges.
        return true;
    }
};

// This policy only permits access to properties that are safe to be used
// across origins.
struct CrossOriginAccessiblePropertiesOnly : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act) {
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
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act);

    static bool deny(js::Wrapper::Action act, JS::HandleId id) {
        // Fail silently for GETs and ENUMERATEs.
        return act == js::Wrapper::GET || act == js::Wrapper::ENUMERATE;
    }
    static bool allowNativeCall(JSContext *cx, JS::IsAcceptableThis test, JS::NativeImpl impl);
};

}

#endif /* __AccessCheck_h__ */
