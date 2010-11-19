/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is Mozilla XPConnect  code, released
 * June 30, 2009.
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

#ifndef xpcpublic_h
#define xpcpublic_h

#include "jsapi.h"
#include "jsobj.h"
#include "nsAString.h"
#include "nsIPrincipal.h"
#include "nsWrapperCache.h"

class nsIPrincipal;

nsresult
xpc_CreateGlobalObject(JSContext *cx, JSClass *clasp,
                       nsIPrincipal *principal, nsISupports *ptr,
                       bool wantXrays, JSObject **global,
                       JSCompartment **compartment);

nsresult
xpc_CreateMTGlobalObject(JSContext *cx, JSClass *clasp,
                         nsISupports *ptr, JSObject **global,
                         JSCompartment **compartment);

extern JSBool
XPC_WN_Equality(JSContext *cx, JSObject *obj, const jsval *v, JSBool *bp);

#define IS_WRAPPER_CLASS(clazz)                                               \
    (clazz->ext.equality == js::Valueify(XPC_WN_Equality))

inline JSBool
DebugCheckWrapperClass(JSObject* obj)
{
    NS_ASSERTION(IS_WRAPPER_CLASS(obj->getClass()),
                 "Forgot to check if this is a wrapper?");
    return JS_TRUE;
}

// If IS_WRAPPER_CLASS for the JSClass of an object is true, the object can be
// a slim wrapper, holding a native in its private slot, or a wrappednative
// wrapper, holding the XPCWrappedNative in its private slot. A slim wrapper
// also holds a pointer to its XPCWrappedNativeProto in a reserved slot, we can
// check that slot for a non-void value to distinguish between the two.

// Only use these macros if IS_WRAPPER_CLASS(obj->getClass()) is true.
#define IS_WN_WRAPPER_OBJECT(obj)                                             \
    (DebugCheckWrapperClass(obj) && obj->getSlot(0).isUndefined())
#define IS_SLIM_WRAPPER_OBJECT(obj)                                           \
    (DebugCheckWrapperClass(obj) && !obj->getSlot(0).isUndefined())

// Use these macros if IS_WRAPPER_CLASS(obj->getClass()) might be false.
// Avoid calling them if IS_WRAPPER_CLASS(obj->getClass()) can only be
// true, as we'd do a redundant call to IS_WRAPPER_CLASS.
#define IS_WN_WRAPPER(obj)                                                    \
    (IS_WRAPPER_CLASS(obj->getClass()) && IS_WN_WRAPPER_OBJECT(obj))
#define IS_SLIM_WRAPPER(obj)                                                  \
    (IS_WRAPPER_CLASS(obj->getClass()) && IS_SLIM_WRAPPER_OBJECT(obj))

inline JSObject *
xpc_GetGlobalForObject(JSObject *obj)
{
    while(JSObject *parent = obj->getParent())
        obj = parent;
    return obj;
}

inline JSObject*
xpc_GetCachedSlimWrapper(nsWrapperCache *cache, JSObject *scope, jsval *vp)
{
    if (cache) {
        JSObject* wrapper = cache->GetWrapper();
        // FIXME: Bug 585786, the check for IS_SLIM_WRAPPER_OBJECT should go
        //        away
        if (wrapper &&
            IS_SLIM_WRAPPER_OBJECT(wrapper) &&
            wrapper->getCompartment() == scope->getCompartment()) {
            *vp = OBJECT_TO_JSVAL(wrapper);

            return wrapper;
        }
    }

    return nsnull;
}

#endif
