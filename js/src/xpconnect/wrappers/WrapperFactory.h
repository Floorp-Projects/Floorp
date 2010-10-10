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

namespace xpc {

class WrapperFactory {
  public:
    enum { WAIVE_XRAY_WRAPPER_FLAG = (1<<0),
           IS_XRAY_WRAPPER_FLAG = (1<<1),
           SCRIPT_ACCESS_ONLY_FLAG = (1<<2),
           PARTIALLY_TRANSPARENT = (1<<3) };

    // Return true if any of any of the nested wrappers have the flag set.
    static bool HasWrapperFlag(JSObject *wrapper, uintN flag) {
        uintN flags = 0;
        wrapper->unwrap(&flags);
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

    // Prepare a given object for wrapping in a new compartment.
    static JSObject *PrepareForWrapping(JSContext *cx,
                                        JSObject *scope,
                                        JSObject *obj,
                                        uintN flags);

    // Rewrap an object that is about to cross compartment boundaries.
    static JSObject *Rewrap(JSContext *cx,
                            JSObject *obj,
                            JSObject *wrappedProto,
                            JSObject *parent,
                            uintN flags);

    // Return true if this is a location object.
    static bool IsLocationObject(JSObject *obj);

    // Wrap a location object.
    static JSObject *WrapLocationObject(JSContext *cx, JSObject *obj);
};

extern JSWrapper WaiveXrayWrapperWrapper;

}
