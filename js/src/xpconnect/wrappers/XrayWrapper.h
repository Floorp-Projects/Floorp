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

// Xray wrappers re-resolve the original native properties on the native
// object and always directly access to those properties.
// Because they work so differently from the rest of the wrapper hierarchy,
// we pull them out of the JSWrapper inheritance hierarchy and create a
// little world around them.

namespace xpc {

extern JSClass HolderClass;

// NB: Base *must* derive from JSProxyHandler
template <typename Base, typename Policy>
class XrayWrapper : public Base {
  public:
    XrayWrapper(int flags);
    virtual ~XrayWrapper();

    /* Fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                       bool set, js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                          bool set, js::PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                     js::AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual bool fix(JSContext *cx, JSObject *proxy, js::Value *vp);

    /* Derived proxy traps. */
    virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                     js::Value *vp);
    virtual bool set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                     js::Value *vp);
    virtual bool has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool enumerateOwn(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *wrapper, uintN flags, js::Value *vp);

    static JSObject *createHolder(JSContext *cx, JSObject *wrappedNative, JSObject *parent);

    static XrayWrapper singleton;
};

class CrossCompartmentXray {
  public:
    static bool enter(JSContext *cx, JSObject *wrapper, jsid *idp,
                      JSWrapper::Action act, void **priv);
    static void leave(JSContext *cx, JSObject *wrapper, void *priv);
};

class SameCompartmentXray {
  public:
    static bool enter(JSContext *, JSObject *, jsid *, JSWrapper::Action, void **) {
        return true;
    }
    static void leave(JSContext *cx, JSObject *wrapper, void *priv) {
    }
};

}
