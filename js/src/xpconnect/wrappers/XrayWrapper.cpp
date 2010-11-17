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
#include "jsiter.h"

#include "XPCWrapper.h"
#include "xpcprivate.h"

namespace xpc {

using namespace js;

static const uint32 JSSLOT_WN_OBJ = 0;
static const uint32 JSSLOT_RESOLVING = 1;
static const uint32 JSSLOT_EXPANDO = 2;

class ResolvingId
{
  public:
    ResolvingId(JSObject *holder, jsid id)
      : mId(id),
        mPrev(getResolvingId(holder)),
        mHolder(holder)
    {
        holder->setSlot(JSSLOT_RESOLVING, PrivateValue(this));
    }

    ~ResolvingId() {
        NS_ASSERTION(getResolvingId(mHolder) == this, "unbalanced ResolvingIds");
        mHolder->setSlot(JSSLOT_RESOLVING, PrivateValue(mPrev));
    }

    static ResolvingId *getResolvingId(JSObject *holder) {
        return (ResolvingId *)holder->getSlot(JSSLOT_RESOLVING).toPrivate();
    }

    jsid mId;
    ResolvingId *mPrev;

  private:
    JSObject *mHolder;
};

static bool
IsResolving(JSObject *holder, jsid id)
{
    for (ResolvingId *cur = ResolvingId::getResolvingId(holder); cur; cur = cur->mPrev) {
        if (cur->mId == id)
            return true;
    }

    return false;
}

static JSBool
holder_get(JSContext *cx, JSObject *holder, jsid id, jsval *vp);

static JSBool
holder_set(JSContext *cx, JSObject *holder, jsid id, jsval *vp);

namespace XrayUtils {

JSClass HolderClass = {
    "NativePropertyHolder",
    JSCLASS_HAS_RESERVED_SLOTS(3),
    JS_PropertyStub,        JS_PropertyStub, holder_get,      holder_set,
    JS_EnumerateStub,       JS_ResolveStub,  JS_ConvertStub,  NULL,
    NULL,                   NULL,            NULL,            NULL,
    NULL,                   NULL,            NULL,            NULL
};

}

using namespace XrayUtils;

static JSObject *
GetHolder(JSObject *obj)
{
    return &obj->getProxyExtra().toObject();
}

static XPCWrappedNative *
GetWrappedNative(JSObject *obj)
{
    NS_ASSERTION(IS_WN_WRAPPER_OBJECT(obj), "expected a wrapped native here");
    return static_cast<XPCWrappedNative *>(obj->getPrivate());
}

static JSObject *
GetWrappedNativeObjectFromHolder(JSContext *cx, JSObject *holder)
{
    NS_ASSERTION(holder->getJSClass() == &HolderClass, "expected a native property holder object");
    JSObject *wrappedObj = &holder->getSlot(JSSLOT_WN_OBJ).toObject();
    OBJ_TO_INNER_OBJECT(cx, wrappedObj);
    return wrappedObj;
}

static JSObject *
GetExpandoObject(JSContext *cx, JSObject *holder)
{
    JSObject *expando = holder->getSlot(JSSLOT_EXPANDO).toObjectOrNull();
    if (!expando) {
        expando =  JS_NewObjectWithGivenProto(cx, nsnull, nsnull, holder->getParent());
        holder->setSlot(JSSLOT_EXPANDO, ObjectValue(*expando));
    }
    return expando;
}

// Some DOM objects have shared properties that don't have an explicit
// getter/setter and rely on the class getter/setter. We install a
// class getter/setter on the holder object to trigger them.
static JSBool
holder_get(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    NS_ASSERTION(wrapper->isProxy(), "bad this object in get");
    JSObject *holder = GetHolder(wrapper);

    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);
    if (NATIVE_HAS_FLAG(wn, WantGetProperty)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, holder))
            return false;
        JSBool retval = true;
        nsresult rv = wn->GetScriptableCallback()->GetProperty(wn, cx, wrapper, id, vp, &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

static JSBool
holder_set(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    NS_ASSERTION(wrapper->isProxy(), "bad this object in set");
    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        return true;
    }

    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

    XPCWrappedNative *wn = GetWrappedNative(wnObject);
    if (NATIVE_HAS_FLAG(wn, WantSetProperty)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, holder))
            return false;
        JSBool retval = true;
        nsresult rv = wn->GetScriptableCallback()->SetProperty(wn, cx, wrapper, id, vp, &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

static bool
ResolveNativeProperty(JSContext *cx, JSObject *wrapper, JSObject *holder, jsid id, bool set,
                      JSPropertyDescriptor *desc)
{
    desc->obj = NULL;

    NS_ASSERTION(holder->getJSClass() == &HolderClass, "expected a native property holder object");
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);

    // This will do verification and the method lookup for us.
    XPCCallContext ccx(JS_CALLER, cx, wnObject, nsnull, id);

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
    desc->shortid = 0;
    desc->value = JSVAL_VOID;

    jsval fval = JSVAL_VOID;
    if (member->IsConstant()) {
        if (!member->GetConstantValue(ccx, iface, &desc->value)) {
            JS_ReportError(cx, "Failed to convert constant native property to JS value");
            return false;
        }
    } else if (member->IsAttribute()) {
        // This is a getter/setter. Clone a function for it.

        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        if (!member->NewFunctionObject(ccx, iface, wnObject, &fval)) {
            JS_ReportError(cx, "Failed to clone function object for native getter/setter");
            return false;
        }

        desc->attrs |= JSPROP_GETTER;
        if (member->IsWritableAttribute())
            desc->attrs |= JSPROP_SETTER;

        // Make the property shared on the holder so no slot is allocated
        // for it. This avoids keeping garbage alive through that slot.
        desc->attrs |= JSPROP_SHARED;
    } else {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

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

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, holder))
        return false;

    if (!JS_WrapValue(cx, &desc->value) || !JS_WrapValue(cx, &fval))
        return false;

    if (desc->attrs & JSPROP_GETTER)
        desc->getter = CastAsJSPropertyOp(JSVAL_TO_OBJECT(fval));
    if (desc->attrs & JSPROP_SETTER)
        desc->setter = desc->getter;

    // Define the property.
    return JS_DefinePropertyById(cx, holder, id, desc->value,
                                 desc->getter, desc->setter, desc->attrs);
}

static JSBool
wrappedJSObject_getter(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    *vp = OBJECT_TO_JSVAL(wrapper);

    return WrapperFactory::WaiveXrayAndWrap(cx, vp);
}

static JSBool
XrayToString(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *wrapper = JS_THIS_OBJECT(cx, vp);
    if (!wrapper->isWrapper() || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }
    JSObject *holder = GetHolder(wrapper);
    JSObject *wrappednative = GetWrappedNativeObjectFromHolder(cx, holder);
    XPCWrappedNative *wn = GetWrappedNative(wrappednative);

    XPCCallContext ccx(JS_CALLER, cx, wrappednative);
    char *wrapperStr = wn->ToString(ccx);
    if (!wrapperStr) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    nsAutoString result(NS_LITERAL_STRING("[object XrayWrapper "));
    result.AppendASCII(wrapperStr);
    JS_smprintf_free(wrapperStr);
    result.Append(']');

    JSString *str = JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar *>(result.get()),
                                        result.Length());
    if (!str)
        return false;

    *vp = STRING_TO_JSVAL(str);
    return true;
}

template <typename Base, typename Policy>
XrayWrapper<Base, Policy>::XrayWrapper(uintN flags)
  : Base(flags | WrapperFactory::IS_XRAY_WRAPPER_FLAG)
{
}

template <typename Base, typename Policy>
XrayWrapper<Base, Policy>::~XrayWrapper()
{
}

template <typename Base, typename Policy>
class AutoLeaveHelper
{
  public:
    AutoLeaveHelper(XrayWrapper<Base, Policy> &xray, JSContext *cx, JSObject *wrapper)
      : xray(xray), cx(cx), wrapper(wrapper)
    {
    }
    ~AutoLeaveHelper()
    {
        xray.leave(cx, wrapper);
    }

  private:
    XrayWrapper<Base, Policy> &xray;
    JSContext *cx;
    JSObject *wrapper;
};

static bool
Transparent(JSContext *cx, JSObject *wrapper)
{
    if (WrapperFactory::HasWaiveXrayFlag(wrapper))
        return true;

    if (!WrapperFactory::IsPartiallyTransparent(wrapper))
        return false;

    // Redirect access straight to the wrapper if UniversalXPConnect is enabled.
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (ssm) {
        PRBool privileged;
        if (NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalXPConnect", &privileged)) && privileged)
            return true;
    }

    return AccessCheck::documentDomainMakesSameOrigin(cx, wrapper->unwrap());
}

namespace XrayUtils {

bool
IsTransparent(JSContext *cx, JSObject *wrapper)
{
    return Transparent(cx, wrapper);
}

}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::resolveOwnProperty(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                              PropertyDescriptor *desc_in)
{
    JSPropertyDescriptor *desc = Jsvalify(desc_in);

    if (id == nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
        if (!this->enter(cx, wrapper, id, set ? JSWrapper::SET : JSWrapper::GET))
            return false;

        AutoLeaveHelper<Base, Policy> helper(*this, cx, wrapper);

        desc->obj = wrapper;
        desc->attrs = JSPROP_ENUMERATE|JSPROP_SHARED;
        desc->getter = wrappedJSObject_getter;
        desc->setter = NULL;
        desc->shortid = 0;
        desc->value = JSVAL_VOID;
        return true;
    }

    JSObject *holder = GetHolder(wrapper);
    JSObject *expando = GetExpandoObject(cx, holder);
    if (!expando)
        return false;

    if (!JS_GetPropertyDescriptorById(cx, expando, id,
                                      (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                      desc)) {
        return false;
    }

    if (desc->obj)
        return true;

    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);

    // Run the resolve hook of the wrapped native.
    if (NATIVE_HAS_FLAG(wn, WantNewResolve)) {
        JSBool retval = true;
        JSObject *pobj = NULL;
        uintN flags = (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->NewResolve(wn, cx, wrapper, id,
                                                                         flags, &pobj, &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }

        if (pobj) {
#ifdef DEBUG
            JSBool hasProp;
            NS_ASSERTION(JS_HasPropertyById(cx, holder, id, &hasProp) &&
                         hasProp, "id got defined somewhere else?");
#endif
            if (!JS_GetPropertyDescriptorById(cx, holder, id, flags, desc))
                return false;
            desc->obj = wrapper;
            return true;
        }
    }

    desc->obj = nsnull;
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                 bool set, PropertyDescriptor *desc_in)
{
    JSPropertyDescriptor *desc = Jsvalify(desc_in);
    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        desc->obj = NULL;
        return true;
    }

    if (!this->enter(cx, wrapper, id, set ? JSWrapper::SET : JSWrapper::GET))
        return false;

    AutoLeaveHelper<Base, Policy> helper(*this, cx, wrapper);

    ResolvingId resolving(holder, id);

    // Redirect access straight to the wrapper if we should be transparent.
    if (Transparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

        {
            JSAutoEnterCompartment ac;
            if (!ac.enter(cx, wnObject))
                return false;

            if (!JS_GetPropertyDescriptorById(cx, wnObject, id,
                                              (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                              desc))
                return false;
        }

        desc->obj = wrapper;
        return cx->compartment->wrap(cx, desc_in);
    }

    if (!this->resolveOwnProperty(cx, wrapper, id, set, desc_in))
        return false;

    if (desc->obj)
        return true;

    void *priv;
    if (!Policy::enter(cx, wrapper, &id, set ? JSWrapper::SET : JSWrapper::GET, &priv))
        return false;

    bool ok = ResolveNativeProperty(cx, wrapper, holder, id, set, desc);
    Policy::leave(cx, wrapper, priv);
    if (!ok || desc->obj)
        return ok;

    if (id == nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING)) {
        desc->obj = wrapper;
        desc->attrs = 0;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->shortid = 0;

        JSObject *toString = JS_NewFunction(cx, XrayToString, 0, 0, holder, "toString");
        if (!toString)
            return false;
        desc->value = OBJECT_TO_JSVAL(toString);
        return true;
    }

    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                    bool set, PropertyDescriptor *desc_in)
{
    JSPropertyDescriptor *desc = Jsvalify(desc_in);
    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        desc->obj = NULL;
        return true;
    }

    if (!this->enter(cx, wrapper, id, set ? JSWrapper::SET : JSWrapper::GET))
        return false;

    AutoLeaveHelper<Base, Policy> helper(*this, cx, wrapper);

    ResolvingId resolving(holder, id);

    // NB: Nothing we do here acts on the wrapped native itself, so we don't
    // enter our policy.
    // Redirect access straight to the wrapper if we should be transparent.
    if (Transparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

        {
            JSAutoEnterCompartment ac;
            if (!ac.enter(cx, wnObject))
                return false;

            if (!JS_GetPropertyDescriptorById(cx, wnObject, id,
                                              (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                              desc)) {
                return false;
            }
        }

        desc->obj = (desc->obj == wnObject) ? wrapper : nsnull;
        return cx->compartment->wrap(cx, desc_in);
    }

    return this->resolveOwnProperty(cx, wrapper, id, set, desc_in);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                          js::PropertyDescriptor *desc)
{
    JSObject *holder = GetHolder(wrapper);
    JSPropertyDescriptor *jsdesc = Jsvalify(desc);

    // Redirect access straight to the wrapper if we should be transparent.
    if (Transparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        if (!cx->compartment->wrap(cx, desc))
            return false;

        return JS_DefinePropertyById(cx, wnObject, id, jsdesc->value, jsdesc->getter, jsdesc->setter,
                                     jsdesc->attrs);
    }

    PropertyDescriptor existing_desc;
    if (!getOwnPropertyDescriptor(cx, wrapper, id, true, &existing_desc))
        return false;

    if (existing_desc.obj && (existing_desc.attrs & JSPROP_PERMANENT))
        return true; // silently ignore attempt to overwrite native property

    if (IsResolving(holder, id)) {
        if (!(jsdesc->attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
            if (!desc->getter)
                jsdesc->getter = holder_get;
            if (!desc->setter)
                jsdesc->setter = holder_set;
        }

        return JS_DefinePropertyById(cx, holder, id, jsdesc->value, jsdesc->getter, jsdesc->setter,
                                     jsdesc->attrs);
    }

    JSObject *expando = GetExpandoObject(cx, holder);
    if (!expando)
        return false;

    return JS_DefinePropertyById(cx, expando, id, jsdesc->value, jsdesc->getter, jsdesc->setter,
                                 jsdesc->attrs);
}

static bool
EnumerateNames(JSContext *cx, JSObject *wrapper, uintN flags, js::AutoIdVector &props)
{
    JSObject *holder = GetHolder(wrapper);

    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

    // Redirect access straight to the wrapper if we should be transparent.
    if (Transparent(cx, wrapper)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        return js::GetPropertyNames(cx, wnObject, flags, &props);
    }

    // Enumerate expando properties first.
    JSObject *expando = GetExpandoObject(cx, holder);
    if (!expando)
        return false;

    if (!js::GetPropertyNames(cx, expando, flags, &props))
        return false;

    // Force all native properties to be materialized onto the wrapped native.
    js::AutoIdVector wnProps(cx);
    {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;
        if (!js::GetPropertyNames(cx, wnObject, flags, &wnProps))
            return false;
    }

    // Go through the properties we got and enumerate all native ones.
    for (size_t n = 0; n < wnProps.length(); ++n) {
        jsid id = wnProps[n];
        JSBool hasProp;
        if (!JS_HasPropertyById(cx, wrapper, id, &hasProp))
            return false;
        if (hasProp)
            props.append(id);
    }
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                               js::AutoIdVector &props)
{
    return EnumerateNames(cx, wrapper, JSITER_OWNONLY | JSITER_HIDDEN, props);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JSObject *holder = GetHolder(wrapper);
    jsval v;
    JSBool b;

    // Redirect access straight to the wrapper if we should be transparent.
    if (Transparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        if (!JS_DeletePropertyById2(cx, wnObject, id, &v) || !JS_ValueToBoolean(cx, v, &b))
            return false;
        *bp = !!b;
        return true;
    }

    JSObject *expando = GetExpandoObject(cx, holder);
    if (!expando)
        return false;

    if (!JS_DeletePropertyById2(cx, expando, id, &v) || !JS_ValueToBoolean(cx, v, &b))
        return false;
    *bp = !!b;
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props)
{
    return EnumerateNames(cx, wrapper, 0, props);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::fix(JSContext *cx, JSObject *proxy, js::Value *vp)
{
    vp->setUndefined();
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                               js::Value *vp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return JSProxyHandler::get(cx, wrapper, wrapper, id, vp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                               js::Value *vp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return JSProxyHandler::set(cx, wrapper, wrapper, id, vp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::has(cx, wrapper, id, bp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::hasOwn(cx, wrapper, id, bp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::enumerateOwn(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::enumerateOwn(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::iterate(JSContext *cx, JSObject *wrapper, uintN flags, js::Value *vp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Policy>
JSObject *
XrayWrapper<Base, Policy>::createHolder(JSContext *cx, JSObject *wrappedNative, JSObject *parent)
{
    JSObject *holder = JS_NewObjectWithGivenProto(cx, &HolderClass, nsnull, parent);
    if (!holder)
        return nsnull;

    JS_ASSERT(IS_WN_WRAPPER(wrappedNative) ||
              wrappedNative->getClass()->ext.innerObject);
    holder->setSlot(JSSLOT_WN_OBJ, ObjectValue(*wrappedNative));
    holder->setSlot(JSSLOT_RESOLVING, PrivateValue(NULL));
    holder->setSlot(JSSLOT_EXPANDO, NullValue());
    return holder;
}

bool
CrossCompartmentXray::enter(JSContext *cx, JSObject *wrapper, jsid *idp,
                            JSWrapper::Action act, void **priv)
{
    JSObject *target = wrapper->unwrap();
    JSCrossCompartmentCall *call = JS_EnterCrossCompartmentCall(cx, target);
    if (!call)
        return false;

    *priv = call;
    return true;
}

void
CrossCompartmentXray::leave(JSContext *cx, JSObject *wrapper, void *priv)
{
    JS_LeaveCrossCompartmentCall(static_cast<JSCrossCompartmentCall *>(priv));
}

#define XPCNW XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray>
#define SCNW XrayWrapper<JSWrapper, SameCompartmentXray>

template <> XPCNW XPCNW::singleton(0);
template <> SCNW SCNW::singleton(0);

template class XPCNW;
template class SCNW;

}
