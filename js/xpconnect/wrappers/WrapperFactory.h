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

extern js::Wrapper WaiveXrayWrapperWrapper;

}

#endif /* _xpc_WRAPPERFACTORY_H */
