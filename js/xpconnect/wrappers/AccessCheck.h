/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code, released
 * June 24, 2010.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsapi.h"
#include "jswrapper.h"
#include "WrapperFactory.h"

class nsIPrincipal;

namespace xpc {

class AccessCheck {
  public:
    static bool isSameOrigin(JSCompartment *a, JSCompartment *b);
    static bool isChrome(JSCompartment *compartment);
    static nsIPrincipal *getPrincipal(JSCompartment *compartment);
    static bool isCrossOriginAccessPermitted(JSContext *cx, JSObject *obj, jsid id,
                                             js::Wrapper::Action act);
    static bool isSystemOnlyAccessPermitted(JSContext *cx);
    static bool isLocationObjectSameOrigin(JSContext *cx, JSObject *wrapper);
    static bool documentDomainMakesSameOrigin(JSContext *cx, JSObject *obj);

    static bool needsSystemOnlyWrapper(JSObject *obj);

    static bool isScriptAccessOnly(JSContext *cx, JSObject *wrapper);

    static void deny(JSContext *cx, jsid id);
};

struct Policy {
    typedef js::Wrapper::Permission Permission;

    static const Permission PermitObjectAccess = js::Wrapper::PermitObjectAccess;
    static const Permission PermitPropertyAccess = js::Wrapper::PermitPropertyAccess;
    static const Permission DenyAccess = js::Wrapper::DenyAccess;
};

// This policy permits access to all properties.
struct Permissive : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act,
                      Permission &perm) {
        perm = PermitObjectAccess;
        return true;
    }
};

// This policy only permits access to the object if the subject can touch
// system objects.
struct OnlyIfSubjectIsSystem : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act,
                      Permission &perm) {
        if (AccessCheck::isSystemOnlyAccessPermitted(cx)) {
            perm = PermitObjectAccess;
            return true;
        }
        perm = DenyAccess;
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wrapper))
            return false;
        AccessCheck::deny(cx, id);
        return false;
    }
};

// This policy only permits access to properties that are safe to be used
// across origins.
struct CrossOriginAccessiblePropertiesOnly : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act,
                      Permission &perm) {
        // Location objects should always use LocationPolicy.
        MOZ_ASSERT(!WrapperFactory::IsLocationObject(js::UnwrapObject(wrapper)));

        if (AccessCheck::isCrossOriginAccessPermitted(cx, wrapper, id, act)) {
            perm = PermitPropertyAccess;
            return true;
        }
        perm = DenyAccess;
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wrapper))
            return false;
        AccessCheck::deny(cx, id);
        return false;
    }
};

// We need a special security policy for Location objects.
//
// Location objects are special because their effective principal is that of
// the outer window, not the inner window. So while the security characteristics
// of most objects can be inferred from their compartments, those of the Location
// object cannot. This has two implications:
//
// 1 - Same-compartment access of Location objects is not necessarily allowed.
//     This means that objects must see a security wrapper around Location objects
//     in their own compartment.
// 2 - Cross-origin access of Location objects is not necessarily forbidden.
//     Since the security decision depends on the current state of the outer window,
//     we can't make it at wrap time. Instead, we need to make it at the time of
//     access.
//
// So for any Location object access, be it same-compartment or cross-compartment,
// we need to do a dynamic security check to determine whether the outer window is
// same-origin with the caller.
//
// So this policy first checks whether the access is something that any code,
// same-origin or not, is allowed to make. If it isn't, it _also_ checks the
// state of the outer window to determine whether we happen to be same-origin
// at the moment.
struct LocationPolicy : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act,
                      Permission &perm) {
        // We should only be dealing with Location objects here.
        MOZ_ASSERT(WrapperFactory::IsLocationObject(js::UnwrapObject(wrapper)));

        // Default to deny.
        perm = DenyAccess;

        // Location object security is complicated enough. Don't allow punctures.
        if (act == js::Wrapper::PUNCTURE)
            return false;

        if (AccessCheck::isCrossOriginAccessPermitted(cx, wrapper, id, act) ||
            AccessCheck::isLocationObjectSameOrigin(cx, wrapper)) {
            perm = PermitPropertyAccess;
            return true;
        }
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wrapper))
            return false;
        AccessCheck::deny(cx, id);
        return false;
    }
};

// This policy only permits access to properties if they appear in the
// objects exposed properties list.
struct ExposedPropertiesOnly : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act,
                      Permission &perm);
};

// Components specific policy
struct ComponentsObjectPolicy : public Policy {
    static bool check(JSContext *cx, JSObject *wrapper, jsid id, js::Wrapper::Action act,
                      Permission &perm);
};

}
