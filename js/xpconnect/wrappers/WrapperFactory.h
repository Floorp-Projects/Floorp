/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _xpc_WRAPPERFACTORY_H
#define _xpc_WRAPPERFACTORY_H

#include "jswrapper.h"

namespace xpc {

class WrapperFactory {
  public:
    enum { WAIVE_XRAY_WRAPPER_FLAG = js::Wrapper::LAST_USED_FLAG << 1,
           IS_XRAY_WRAPPER_FLAG    = WAIVE_XRAY_WRAPPER_FLAG << 1 };

    // Return true if any of any of the nested wrappers have the flag set.
    static bool HasWrapperFlag(JSObject* wrapper, unsigned flag) {
        unsigned flags = 0;
        js::UncheckedUnwrap(wrapper, true, &flags);
        return !!(flags & flag);
    }

    static bool IsXrayWrapper(JSObject* wrapper) {
        return HasWrapperFlag(wrapper, IS_XRAY_WRAPPER_FLAG);
    }

    static bool HasWaiveXrayFlag(JSObject* wrapper) {
        return HasWrapperFlag(wrapper, WAIVE_XRAY_WRAPPER_FLAG);
    }

    static bool IsCOW(JSObject* wrapper);

    static JSObject* GetXrayWaiver(JS::HandleObject obj);
    static JSObject* CreateXrayWaiver(JSContext* cx, JS::HandleObject obj);
    static JSObject* WaiveXray(JSContext* cx, JSObject* obj);

    // Computes whether we should allow the creation of an Xray waiver from
    // |target| to |origin|.
    static bool AllowWaiver(JSCompartment* target, JSCompartment* origin);

    // Convenience method for the above, operating on a wrapper.
    static bool AllowWaiver(JSObject* wrapper);

    // Prepare a given object for wrapping in a new compartment.
    static void PrepareForWrapping(JSContext* cx,
                                   JS::HandleObject scope,
                                   JS::HandleObject obj,
                                   JS::HandleObject objectPassedToWrap,
                                   JS::MutableHandleObject retObj);

    // Rewrap an object that is about to cross compartment boundaries.
    static JSObject* Rewrap(JSContext* cx,
                            JS::HandleObject existing,
                            JS::HandleObject obj);

    // Wrap wrapped object into a waiver wrapper and then re-wrap it.
    static bool WaiveXrayAndWrap(JSContext* cx, JS::MutableHandleValue vp);
    static bool WaiveXrayAndWrap(JSContext* cx, JS::MutableHandleObject object);
};

extern const js::Wrapper XrayWaiver;

} // namespace xpc

#endif /* _xpc_WRAPPERFACTORY_H */
