/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _xpc_WRAPPERFACTORY_H
#define _xpc_WRAPPERFACTORY_H

#include "jsapi.h"
#include "jswrapper.h"

namespace xpc {

class WrapperFactory {
  public:
    enum { WAIVE_XRAY_WRAPPER_FLAG = js::Wrapper::LAST_USED_FLAG << 1,
           IS_XRAY_WRAPPER_FLAG    = WAIVE_XRAY_WRAPPER_FLAG << 1,
           SCRIPT_ACCESS_ONLY_FLAG = IS_XRAY_WRAPPER_FLAG << 1,
           PARTIALLY_TRANSPARENT   = SCRIPT_ACCESS_ONLY_FLAG << 1,
           SOW_FLAG                = PARTIALLY_TRANSPARENT << 1,

           // Prevent scripts from shadowing native properties.
           // NB: Applies only to Xray wrappers.
           // NB: This will prevent scriptable helpers from defining special
           //     handlers for properties defined in IDL. Use with caution.
           SHADOWING_FORBIDDEN     = SOW_FLAG << 1 };

    // Return true if any of any of the nested wrappers have the flag set.
    static bool HasWrapperFlag(JSObject *wrapper, unsigned flag) {
        unsigned flags = 0;
        js::UnwrapObject(wrapper, true, &flags);
        return !!(flags & flag);
    }

    static bool IsXrayWrapper(JSObject *wrapper) {
        return HasWrapperFlag(wrapper, IS_XRAY_WRAPPER_FLAG);
    }

    static bool IsPartiallyTransparent(JSObject *wrapper) {
        return HasWrapperFlag(wrapper, PARTIALLY_TRANSPARENT);
    }

    static bool HasWaiveXrayFlag(JSObject *wrapper) {
        return HasWrapperFlag(wrapper, WAIVE_XRAY_WRAPPER_FLAG);
    }

    static bool IsShadowingForbidden(JSObject *wrapper) {
        return HasWrapperFlag(wrapper, SHADOWING_FORBIDDEN);
    }

    static JSObject *WaiveXray(JSContext *cx, JSObject *obj);

    static JSObject *DoubleWrap(JSContext *cx, JSObject *obj, unsigned flags);

    // Prepare a given object for wrapping in a new compartment.
    static JSObject *PrepareForWrapping(JSContext *cx,
                                        JSObject *scope,
                                        JSObject *obj,
                                        unsigned flags);

    // Rewrap an object that is about to cross compartment boundaries.
    static JSObject *Rewrap(JSContext *cx,
                            JSObject *obj,
                            JSObject *wrappedProto,
                            JSObject *parent,
                            unsigned flags);

    // Wrap an object for same-compartment access.
    static JSObject *WrapForSameCompartment(JSContext *cx,
                                            JSObject *obj);

    // Return true if this is a location object.
    static bool IsLocationObject(JSObject *obj);

    // Wrap a location object.
    static JSObject *WrapLocationObject(JSContext *cx, JSObject *obj);

    // Wrap wrapped object into a waiver wrapper and then re-wrap it.
    static bool WaiveXrayAndWrap(JSContext *cx, jsval *vp);

    // Wrap a (same compartment) object in a SOW.
    static JSObject *WrapSOWObject(JSContext *cx, JSObject *obj);

    // Return true if this is a Components object.
    static bool IsComponentsObject(JSObject *obj);

    // Wrap a (same compartment) Components object.
    static JSObject *WrapComponentsObject(JSContext *cx, JSObject *obj);
};

extern js::DirectWrapper WaiveXrayWrapperWrapper;

}

#endif /* _xpc_WRAPPERFACTORY_H */
