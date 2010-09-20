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

#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "FilteringWrapper.h"
#include "CrossOriginWrapper.h"
#include "WrapperFactory.h"

#include "jscntxt.h"

#include "XPCWrapper.h"
#include "xpcprivate.h"

namespace xpc {

using namespace js;

static const uint32 JSSLOT_WN_OBJ = 0;

static JSBool
holder_get(JSContext *cx, JSObject *holder, jsid id, jsval *vp);

static JSBool
holder_set(JSContext *cx, JSObject *holder, jsid id, jsval *vp);

static JSBool
holder_enumerate(JSContext *cx, JSObject *holder);

JSClass HolderClass = {
    "NativePropertyHolder",
    JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,        JS_PropertyStub, holder_get,      holder_set,
    holder_enumerate,       JS_ResolveStub,  JS_ConvertStub,  NULL,
    NULL,                   NULL,            NULL,            NULL,
    NULL,                   NULL,            NULL,            NULL
};

template <typename Base>
XrayWrapper<Base>::XrayWrapper(uintN flags) : Base(flags)
{
}

template <typename Base>
XrayWrapper<Base>::~XrayWrapper()
{
}

static XPCWrappedNative *
GetWrappedNative(JSObject *obj)
{
    NS_ASSERTION(IS_WN_WRAPPER_OBJECT(obj), "expected a wrapped native here");
    return static_cast<XPCWrappedNative *>(obj->getPrivate());
}

static JSObject *
GetWrappedNativeObjectFromHolder(JSObject *holder)
{
    NS_ASSERTION(holder->getJSClass() == &HolderClass, "expected a native property holder object");
    return holder->getSlot(JSSLOT_WN_OBJ).toObjectOrNull();
}

// Some DOM objects have shared properties that don't have an explicit
// getter/setter and rely on the class getter/setter. We install a
// class getter/setter on the holder object to trigger them.
static JSBool
holder_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(obj);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);
    if (NATIVE_HAS_FLAG(wn, WantGetProperty)) {
        JSBool retval = true;
        nsresult rv = wn->GetScriptableCallback()->GetProperty(wn, cx, obj, id, vp, &retval);
        if (NS_FAILED(rv)) {
            if (retval) {
                XPCThrower::Throw(rv, cx);
            }
            return false;
        }
    }
    return true;
}

static JSBool
holder_set(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(obj);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);
    if (NATIVE_HAS_FLAG(wn, WantSetProperty)) {
        JSBool retval = true;
        nsresult rv = wn->GetScriptableCallback()->SetProperty(wn, cx, obj, id, vp, &retval);
        if (NS_FAILED(rv)) {
            if (retval) {
                XPCThrower::Throw(rv, cx);
            }
            return false;
        }
    }
    return true;
}

static bool
ResolveNativeProperty(JSContext *cx, JSObject *holder, jsid id, bool set, JSPropertyDescriptor *desc)
{
    desc->obj = NULL;

    NS_ASSERTION(holder->getJSClass() == &HolderClass, "expected a native property holder object");
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);

    // This will do verification and the method lookup for us.
    XPCCallContext ccx(JS_CALLER, cx, holder, nsnull, id);

    // Run the resolve hook of the wrapped native.
    JSBool retval = true;
    JSObject *pobj = NULL;
    uintN flags = cx->resolveFlags | (set ? JSRESOLVE_ASSIGNING : 0);
    nsresult rv = wn->GetScriptableInfo()->GetCallback()->NewResolve(wn, cx, holder, id, flags,
                                                                     &pobj, &retval);
    if (NS_FAILED(rv)) {
        if (retval) {
            XPCThrower::Throw(rv, cx);
        }
        return false;
    }

    if (pobj) {
        return JS_GetPropertyDescriptorById(cx, pobj, id, cx->resolveFlags, desc);
    }

    // There are no native numeric properties, so we can shortcut here. We will not
    // find the property.
    if (!JSID_IS_ATOM(id)) {
        /* Not found */
        return true;
    }

    XPCNativeInterface *iface;
    XPCNativeMember *member;
    if (ccx.GetWrapper() != wn ||
       !wn->IsValid()  ||
       !(iface = ccx.GetInterface()) ||
       !(member = ccx.GetMember())) {
        /* Not found */
        return true;
    }

    desc->obj = holder;
    desc->attrs = JSPROP_ENUMERATE;
    desc->getter = NULL;
    desc->setter = NULL;
    desc->shortid = NULL;
    desc->value = JSVAL_VOID;

    if (member->IsConstant()) {
        if (!member->GetConstantValue(ccx, iface, &desc->value)) {
            JS_ReportError(cx, "Failed to convert constant native property to JS value");
            return false;
        }
    } else if (member->IsAttribute()) {
        // This is a getter/setter. Clone a function for it.
        jsval fval;
        if (!member->NewFunctionObject(ccx, iface, wnObject, &fval)) {
            JS_ReportError(cx, "Failed to clone function object for native getter/setter");
            return false;
        }
        desc->getter = CastAsJSPropertyOp(JSVAL_TO_OBJECT(fval));
        desc->attrs |= JSPROP_GETTER;
        if (member->IsWritableAttribute()) {
            desc->setter = desc->getter;;
            desc->attrs |= JSPROP_SETTER;
        }

        // Make the property shared on the holder so no slot is allocated
        // for it. This avoids keeping garbage alive through that slot.
        desc->attrs |= JSPROP_SHARED;
    } else {
        // This is a method. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wnObject, &desc->value)) {
            JS_ReportError(cx, "Failed to clone function object for native function");
            return false;
        }

        // Without a wrapper the function would live on the prototype. Since we
        // don't have one, we have to avoid calling the scriptable helper's
        // GetProperty method for this property, so stub out the getter and
        // setter here explicitly.
        desc->getter = desc->setter = JS_PropertyStub;
    }

    // Define the property.
    return JS_DefinePropertyById(cx, holder, id, desc->value,
                                 desc->getter, desc->setter, desc->attrs);
}

static JSBool
holder_enumerate(JSContext *cx, JSObject *holder)
{
    // Ask the native wrapper for all its ids
    JSIdArray *ida = JS_Enumerate(cx, GetWrappedNativeObjectFromHolder(holder));
    if (!ida)
        return false;
    jsid *idp = ida->vector;
    size_t length = ida->length;
    // Resolve the underlyign native properties onto the holder object
    while (length-- > 0) {
        JSPropertyDescriptor dummy;
        if (!ResolveNativeProperty(cx, holder, *idp++, false, &dummy))
            return false;
    }
    return true;
}

struct NativePropertiesOnly : public Policy {
    static bool check(JSContext *cx, JSObject *obj, jsid id, bool set, Permission &perm);
};

extern JSWrapper WaiveXrayWrapperWrapper;

static JSBool
wrappedJSObject_getter(JSContext *cx, JSObject *holder, jsid id, jsval *vp)
{
    // If the caller intentionally waives the X-ray wrapper we usually
    // apply for wrapped natives, use a special wrapper to make sure the
    // membrane will not automatically apply an X-ray wrapper.
    JSObject *wn = GetWrappedNativeObjectFromHolder(holder);
    JSObject *obj = JSWrapper::New(cx, wn, NULL, wn->getParent(), &WaiveXrayWrapperWrapper);
    if (!obj)
        return false;
    *vp = OBJECT_TO_JSVAL(obj);
    return true;
}

template <typename Base>
bool
XrayWrapper<Base>::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc_in)
{
    JSPropertyDescriptor *desc = Jsvalify(desc_in);

    if (id == nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
        desc->obj = wrapper;
        desc->attrs = JSPROP_ENUMERATE|JSPROP_SHARED;
        desc->getter = wrappedJSObject_getter;
        desc->setter = NULL;
        desc->shortid = NULL;
        desc->value = JSVAL_VOID;
        return true;
    }
    if (!Base::getPropertyDescriptor(cx, wrapper, id, desc_in)) {
        return false;
    }
    if (desc->obj)
        return true;
    return ResolveNativeProperty(cx, Base::wrappedObject(wrapper), id, false, desc);
}

template <typename Base>
bool
XrayWrapper<Base>::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id, PropertyDescriptor *desc)
{
    return getPropertyDescriptor(cx, wrapper, id, desc);
}

template <typename Base>
bool
XrayWrapper<Base>::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Use the default implementation, which forwards to getPropertyDescriptor.
    return JSProxyHandler::has(cx, wrapper, id, bp);
}

template <typename Base>
bool
XrayWrapper<Base>::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Use the default implementation, which forwards to getOwnPropertyDescriptor.
    return JSProxyHandler::hasOwn(cx, wrapper, id, bp);
}

#define SJOW XrayWrapper<JSCrossCompartmentWrapper>
#define XOSJOW XrayWrapper<CrossOriginWrapper>

template <> SJOW SJOW::singleton(0);
template <> XOSJOW XOSJOW::singleton(0);

template class SJOW;
template class XOSJOW;

}
