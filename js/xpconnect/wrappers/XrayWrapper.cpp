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

#include "nsINode.h"
#include "nsIDocument.h"

#include "XPCWrapper.h"
#include "xpcprivate.h"

#include "jsapi.h"

namespace xpc {

using namespace js;

static const uint32_t JSSLOT_WN = 0;
static const uint32_t JSSLOT_RESOLVING = 1;
static const uint32_t JSSLOT_EXPANDO = 2;

class ResolvingId
{
  public:
    ResolvingId(JSObject *holder, jsid id)
      : mId(id),
        mPrev(getResolvingId(holder)),
        mHolder(holder)
    {
        js::SetReservedSlot(holder, JSSLOT_RESOLVING, PrivateValue(this));
    }

    ~ResolvingId() {
        NS_ASSERTION(getResolvingId(mHolder) == this, "unbalanced ResolvingIds");
        js::SetReservedSlot(mHolder, JSSLOT_RESOLVING, PrivateValue(mPrev));
    }

    static ResolvingId *getResolvingId(JSObject *holder) {
        return (ResolvingId *)js::GetReservedSlot(holder, JSSLOT_RESOLVING).toPrivate();
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
holder_set(JSContext *cx, JSObject *holder, jsid id, JSBool strict, jsval *vp);

namespace XrayUtils {

JSClass HolderClass = {
    "NativePropertyHolder",
    JSCLASS_HAS_RESERVED_SLOTS(3),
    JS_PropertyStub,        JS_PropertyStub, holder_get,      holder_set,
    JS_EnumerateStub,       JS_ResolveStub,  JS_ConvertStub
};

}

using namespace XrayUtils;

static JSObject *
GetHolder(JSObject *obj)
{
    return &js::GetProxyExtra(obj, 0).toObject();
}

static XPCWrappedNative *
GetWrappedNative(JSObject *obj)
{
    NS_ASSERTION(IS_WN_WRAPPER_OBJECT(obj), "expected a wrapped native here");
    return static_cast<XPCWrappedNative *>(js::GetObjectPrivate(obj));
}

static XPCWrappedNative *
GetWrappedNativeFromHolder(JSObject *holder)
{
    NS_ASSERTION(js::GetObjectJSClass(holder) == &HolderClass, "expected a native property holder object");
    return static_cast<XPCWrappedNative *>(js::GetReservedSlot(holder, JSSLOT_WN).toPrivate());
}

static JSObject *
GetWrappedNativeObjectFromHolder(JSObject *holder)
{
    NS_ASSERTION(js::GetObjectJSClass(holder) == &HolderClass, "expected a native property holder object");
    return GetWrappedNativeFromHolder(holder)->GetFlatJSObject();
}

static JSObject *
GetExpandoObject(JSObject *holder)
{
    NS_ASSERTION(js::GetObjectJSClass(holder) == &HolderClass, "expected a native property holder object");
    return js::GetReservedSlot(holder, JSSLOT_EXPANDO).toObjectOrNull();
}

static JSObject *
EnsureExpandoObject(JSContext *cx, JSObject *holder)
{
    NS_ASSERTION(js::GetObjectJSClass(holder) == &HolderClass, "expected a native property holder object");
    JSObject *expando = GetExpandoObject(holder);
    if (expando)
        return expando;
    CompartmentPrivate *priv =
        (CompartmentPrivate *)JS_GetCompartmentPrivate(js::GetObjectCompartment(holder));
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    expando = priv->LookupExpandoObject(wn);
    if (!expando) {
        expando = JS_NewObjectWithGivenProto(cx, nsnull, nsnull, js::GetObjectParent(holder));
        if (!expando)
            return NULL;
        // Add the expando object to the expando map to keep it alive.
        if (!priv->RegisterExpandoObject(wn, expando)) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        // Make sure the wn stays alive so it keeps the expando object alive.
        nsRefPtr<nsXPCClassInfo> ci;
        CallQueryInterface(wn->Native(), getter_AddRefs(ci));
        if (ci)
            ci->PreserveWrapper(wn->Native());
    }
    js::SetReservedSlot(holder, JSSLOT_EXPANDO, ObjectValue(*expando));
    return expando;
}

static inline JSObject *
FindWrapper(JSObject *wrapper)
{
    while (!js::IsWrapper(wrapper) ||
           !(Wrapper::wrapperHandler(wrapper)->flags() & WrapperFactory::IS_XRAY_WRAPPER_FLAG)) {
        wrapper = js::GetObjectProto(wrapper);
        // NB: we must eventually hit our wrapper.
    }

    return wrapper;
}

// Some DOM objects have shared properties that don't have an explicit
// getter/setter and rely on the class getter/setter. We install a
// class getter/setter on the holder object to trigger them.
static JSBool
holder_get(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    wrapper = FindWrapper(wrapper);

    JSObject *holder = GetHolder(wrapper);

    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    if (NATIVE_HAS_FLAG(wn, WantGetProperty)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, holder))
            return false;
        bool retval = true;
        nsresult rv = wn->GetScriptableCallback()->GetProperty(wn, cx, wrapper, id, vp, &retval);
        if (NS_FAILED(rv) || !retval) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

static JSBool
holder_set(JSContext *cx, JSObject *wrapper, jsid id, JSBool strict, jsval *vp)
{
    wrapper = FindWrapper(wrapper);

    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        return true;
    }

    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    if (NATIVE_HAS_FLAG(wn, WantSetProperty)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, holder))
            return false;
        bool retval = true;
        nsresult rv = wn->GetScriptableCallback()->SetProperty(wn, cx, wrapper, id, vp, &retval);
        if (NS_FAILED(rv) || !retval) {
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

    NS_ASSERTION(js::GetObjectJSClass(holder) == &HolderClass, "expected a native property holder object");
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);

    // This will do verification and the method lookup for us.
    XPCCallContext ccx(JS_CALLER, cx, wnObject, nsnull, id);

    // There are no native numeric properties, so we can shortcut here. We will not
    // find the property.
    if (!JSID_IS_STRING(id)) {
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
        if (!member->NewFunctionObject(ccx, iface, wrapper, &fval)) {
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
        // This is a method. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wrapper, &desc->value)) {
            JS_ReportError(cx, "Failed to clone function object for native function");
            return false;
        }

        // Without a wrapper the function would live on the prototype. Since we
        // don't have one, we have to avoid calling the scriptable helper's
        // GetProperty method for this property, so stub out the getter and
        // setter here explicitly.
        desc->getter = JS_PropertyStub;
        desc->setter = JS_StrictPropertyStub;
    }

    if (!JS_WrapValue(cx, &desc->value) || !JS_WrapValue(cx, &fval))
        return false;

    if (desc->attrs & JSPROP_GETTER)
        desc->getter = js::CastAsJSPropertyOp(JSVAL_TO_OBJECT(fval));
    if (desc->attrs & JSPROP_SETTER)
        desc->setter = js::CastAsJSStrictPropertyOp(JSVAL_TO_OBJECT(fval));

    // Define the property.
    return JS_DefinePropertyById(cx, holder, id, desc->value,
                                 desc->getter, desc->setter, desc->attrs);
}

static JSBool
wrappedJSObject_getter(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    *vp = OBJECT_TO_JSVAL(wrapper);

    return WrapperFactory::WaiveXrayAndWrap(cx, vp);
}

template <typename T>
static bool
Is(JSObject *wrapper)
{
    JSObject *holder = GetHolder(wrapper);
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    nsCOMPtr<T> native = do_QueryWrappedNative(wn);
    return !!native;
}

static JSBool
WrapURI(JSContext *cx, nsIURI *uri, jsval *vp)
{
    JSObject *scope = JS_GetGlobalForScopeChain(cx);
    nsresult rv =
        nsXPConnect::FastGetXPConnect()->WrapNativeToJSVal(cx, scope, uri, nsnull,
                                                           &NS_GET_IID(nsIURI), true,
                                                           vp, nsnull);
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }
    return true;
}

static JSBool
documentURIObject_getter(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    JSObject *holder = GetHolder(wrapper);
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    nsCOMPtr<nsIDocument> native = do_QueryWrappedNative(wn);
    if (!native) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    nsCOMPtr<nsIURI> uri = native->GetDocumentURI();
    if (!uri) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    return WrapURI(cx, uri, vp);
}

static JSBool
baseURIObject_getter(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    JSObject *holder = GetHolder(wrapper);
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    nsCOMPtr<nsINode> native = do_QueryWrappedNative(wn);
    if (!native) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }
    nsCOMPtr<nsIURI> uri = native->GetBaseURI();
    if (!uri) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    return WrapURI(cx, uri, vp);
}

static JSBool
nodePrincipal_getter(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    JSObject *holder = GetHolder(wrapper);
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
    nsCOMPtr<nsINode> node = do_QueryWrappedNative(wn);
    if (!node) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    JSObject *scope = JS_GetGlobalForScopeChain(cx);
    nsresult rv =
        nsXPConnect::FastGetXPConnect()->WrapNativeToJSVal(cx, scope, node->NodePrincipal(), nsnull,
                                                           &NS_GET_IID(nsIPrincipal), true,
                                                           vp, nsnull);
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }
    return true;
}

static JSBool
XrayToString(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *wrapper = JS_THIS_OBJECT(cx, vp);
    if (!wrapper || !IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }

    nsAutoString result(NS_LITERAL_STRING("[object XrayWrapper "));
    if (mozilla::dom::binding::instanceIsProxy(&js::GetProxyPrivate(wrapper).toObject())) {
        JSString *wrapperStr = js::GetProxyHandler(wrapper)->obj_toString(cx, wrapper);
        size_t length;
        const jschar* chars = JS_GetStringCharsAndLength(cx, wrapperStr, &length);
        if (!chars) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        result.Append(chars, length);
    } else {
        JSObject *holder = GetHolder(wrapper);
        XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);
        JSObject *wrappednative = wn->GetFlatJSObject();

        XPCCallContext ccx(JS_CALLER, cx, wrappednative);
        char *wrapperStr = wn->ToString(ccx);
        if (!wrapperStr) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        result.AppendASCII(wrapperStr);
        JS_smprintf_free(wrapperStr);
    }

    result.Append(']');

    JSString *str = JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar *>(result.get()),
                                        result.Length());
    if (!str)
        return false;

    *vp = STRING_TO_JSVAL(str);
    return true;
}

template <typename Base>
XrayWrapper<Base>::XrayWrapper(unsigned flags)
  : Base(flags | WrapperFactory::IS_XRAY_WRAPPER_FLAG)
{
}

template <typename Base>
XrayWrapper<Base>::~XrayWrapper()
{
}

template <typename Base>
class AutoLeaveHelper
{
  public:
    AutoLeaveHelper(XrayWrapper<Base> &xray, JSContext *cx, JSObject *wrapper)
      : xray(xray), cx(cx), wrapper(wrapper)
    {
    }
    ~AutoLeaveHelper()
    {
        xray.leave(cx, wrapper);
    }

  private:
    XrayWrapper<Base> &xray;
    JSContext *cx;
    JSObject *wrapper;
};

static bool
IsPrivilegedScript()
{
    // Redirect access straight to the wrapper if UniversalXPConnect is enabled.
    nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
    if (ssm) {
        bool privileged;
        if (NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalXPConnect", &privileged)) && privileged)
            return true;
    }
    return false;
}

namespace XrayUtils {

bool
IsTransparent(JSContext *cx, JSObject *wrapper)
{
    if (WrapperFactory::HasWaiveXrayFlag(wrapper))
        return true;

    if (!WrapperFactory::IsPartiallyTransparent(wrapper))
        return false;

    // Redirect access straight to the wrapper if UniversalXPConnect is enabled.
    if (IsPrivilegedScript())
        return true;

    return AccessCheck::documentDomainMakesSameOrigin(cx, UnwrapObject(wrapper));
}

JSObject *
GetNativePropertiesObject(JSContext *cx, JSObject *wrapper)
{
    NS_ASSERTION(js::IsWrapper(wrapper) && WrapperFactory::IsXrayWrapper(wrapper),
                 "bad object passed in");

    JSObject *holder = GetHolder(wrapper);
    NS_ASSERTION(holder, "uninitialized wrapper being used?");
    return holder;
}

}

template <typename Base>
bool
XrayWrapper<Base>::resolveOwnProperty(JSContext *cx, JSObject *wrapper, jsid id, bool set,
                                      PropertyDescriptor *desc)
{
    // Partially transparent wrappers (which used to be known as XOWs) don't
    // have a .wrappedJSObject property.
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if (!WrapperFactory::IsPartiallyTransparent(wrapper) &&
        (id == rt->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT) ||
         // Check for baseURIObject and nodePrincipal no nodes and
         // documentURIObject on documents, but only from privileged scripts.
         // Do the id checks before the QIs and IsPrivilegedScript() checks,
         // since they're cheaper and will tend to fail most of the time
         // anyway.
         ((((id == rt->GetStringID(XPCJSRuntime::IDX_BASEURIOBJECT) ||
             id == rt->GetStringID(XPCJSRuntime::IDX_NODEPRINCIPAL)) &&
            Is<nsINode>(wrapper)) ||
           (id == rt->GetStringID(XPCJSRuntime::IDX_DOCUMENTURIOBJECT) &&
            Is<nsIDocument>(wrapper))) &&
          IsPrivilegedScript()))) {
        bool status;
        Wrapper::Action action = set ? Wrapper::SET : Wrapper::GET;
        desc->obj = NULL; // default value
        if (!this->enter(cx, wrapper, id, action, &status))
            return status;

        AutoLeaveHelper<Base> helper(*this, cx, wrapper);

        desc->obj = wrapper;
        desc->attrs = JSPROP_ENUMERATE|JSPROP_SHARED;
        if (id == rt->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT))
            desc->getter = wrappedJSObject_getter;
        else if (id == rt->GetStringID(XPCJSRuntime::IDX_BASEURIOBJECT))
            desc->getter = baseURIObject_getter;
        else if (id == rt->GetStringID(XPCJSRuntime::IDX_DOCUMENTURIOBJECT))
            desc->getter = documentURIObject_getter;
        else
            desc->getter = nodePrincipal_getter;
        desc->setter = NULL;
        desc->shortid = 0;
        desc->value = JSVAL_VOID;
        return true;
    }

    desc->obj = NULL;

    unsigned flags = (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED;
    JSObject *holder = GetHolder(wrapper);
    JSObject *expando = GetExpandoObject(holder);

    // Check for expando properties first.
    if (expando && !JS_GetPropertyDescriptorById(cx, expando, id, flags, desc)) {
        return false;
    }
    if (desc->obj) {
        // Pretend the property lives on the wrapper.
        desc->obj = wrapper;
        return true;
    }

    JSBool hasProp;
    if (!JS_HasPropertyById(cx, holder, id, &hasProp)) {
        return false;
    }
    if (!hasProp) {
        XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);

        // Run the resolve hook of the wrapped native.
        if (!NATIVE_HAS_FLAG(wn, WantNewResolve)) {
            desc->obj = nsnull;
            return true;
        }

        bool retval = true;
        JSObject *pobj = NULL;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->NewResolve(wn, cx, wrapper, id,
                                                                         flags, &pobj, &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }

        if (!pobj) {
            desc->obj = nsnull;
            return true;
        }

#ifdef DEBUG
        NS_ASSERTION(JS_HasPropertyById(cx, holder, id, &hasProp) &&
                     hasProp, "id got defined somewhere else?");
#endif
    }

    if (!JS_GetPropertyDescriptorById(cx, holder, id, flags, desc))
        return false;

    // Pretend we found the property on the wrapper, not the holder.
    desc->obj = wrapper;

    return true;
}

template <typename Base>
bool
XrayWrapper<Base>::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                         bool set, PropertyDescriptor *desc)
{
    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        desc->obj = NULL;
        return true;
    }

    bool status;
    Wrapper::Action action = set ? Wrapper::SET : Wrapper::GET;
    desc->obj = NULL; // default value
    if (!this->enter(cx, wrapper, id, action, &status))
        return status;

    AutoLeaveHelper<Base> helper(*this, cx, wrapper);

    ResolvingId resolving(holder, id);

    // Redirect access straight to the wrapper if we should be transparent.
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);

        {
            JSAutoEnterCompartment ac;
            if (!ac.enter(cx, wnObject))
                return false;

            if (!JS_GetPropertyDescriptorById(cx, wnObject, id,
                                              (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                              desc))
                return false;
        }

        if (desc->obj)
            desc->obj = wrapper;
        return JS_WrapPropertyDescriptor(cx, desc);
    }

    if (!this->resolveOwnProperty(cx, wrapper, id, set, desc))
        return false;

    if (desc->obj)
        return true;

    bool ok = ResolveNativeProperty(cx, wrapper, holder, id, set, desc);
    if (!ok || desc->obj)
        return ok;

    if (id == nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING)) {
        desc->obj = wrapper;
        desc->attrs = 0;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->shortid = 0;

        JSObject *toString = JS_GetFunctionObject(JS_NewFunction(cx, XrayToString, 0, 0, holder, "toString"));
        if (!toString)
            return false;
        desc->value = OBJECT_TO_JSVAL(toString);
        return true;
    }

    return true;
}

template <typename Base>
bool
XrayWrapper<Base>::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                            bool set, PropertyDescriptor *desc)
{
    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        desc->obj = NULL;
        return true;
    }

    bool status;
    Wrapper::Action action = set ? Wrapper::SET : Wrapper::GET;
    desc->obj = NULL; // default value
    if (!this->enter(cx, wrapper, id, action, &status))
        return status;

    AutoLeaveHelper<Base> helper(*this, cx, wrapper);

    ResolvingId resolving(holder, id);

    // NB: Nothing we do here acts on the wrapped native itself, so we don't
    // enter our policy.
    // Redirect access straight to the wrapper if we should be transparent.
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);

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
        return JS_WrapPropertyDescriptor(cx, desc);
    }

    return this->resolveOwnProperty(cx, wrapper, id, set, desc);
}

template <typename Base>
bool
XrayWrapper<Base>::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                  js::PropertyDescriptor *desc)
{
    JSObject *holder = GetHolder(wrapper);
    JSPropertyDescriptor *jsdesc = desc;

    // Redirect access straight to the wrapper if we should be transparent.
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);

        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        if (!JS_WrapPropertyDescriptor(cx, desc))
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

    JSObject *expando = EnsureExpandoObject(cx, holder);
    if (!expando)
        return false;

    return JS_DefinePropertyById(cx, expando, id, jsdesc->value, jsdesc->getter, jsdesc->setter,
                                 jsdesc->attrs);
}

static bool
EnumerateNames(JSContext *cx, JSObject *wrapper, unsigned flags, JS::AutoIdVector &props)
{
    JSObject *holder = GetHolder(wrapper);

    JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);

    // Redirect access straight to the wrapper if we should be transparent.
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        return js::GetPropertyNames(cx, wnObject, flags, &props);
    }

    if (WrapperFactory::IsPartiallyTransparent(wrapper)) {
        JS_ReportError(cx, "Not allowed to enumerate cross origin objects");
        return false;
    }

    // Enumerate expando properties first.
    JSObject *expando = GetExpandoObject(holder);
    if (expando && !js::GetPropertyNames(cx, expando, flags, &props))
        return false;

    // Force all native properties to be materialized onto the wrapped native.
    JS::AutoIdVector wnProps(cx);
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

template <typename Base>
bool
XrayWrapper<Base>::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                       JS::AutoIdVector &props)
{
    return EnumerateNames(cx, wrapper, JSITER_OWNONLY | JSITER_HIDDEN, props);
}

template <typename Base>
bool
XrayWrapper<Base>::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JSObject *holder = GetHolder(wrapper);
    jsval v;
    JSBool b;

    // Redirect access straight to the wrapper if we should be transparent.
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSObject *wnObject = GetWrappedNativeObjectFromHolder(holder);

        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, wnObject))
            return false;

        if (!JS_DeletePropertyById2(cx, wnObject, id, &v) || !JS_ValueToBoolean(cx, v, &b))
            return false;
        *bp = !!b;
        return true;
    }

    JSObject *expando = GetExpandoObject(holder);
    b = true;
    if (expando &&
        (!JS_DeletePropertyById2(cx, expando, id, &v) ||
         !JS_ValueToBoolean(cx, v, &b))) {
        return false;
    }

    *bp = !!b;
    return true;
}

template <typename Base>
bool
XrayWrapper<Base>::enumerate(JSContext *cx, JSObject *wrapper, JS::AutoIdVector &props)
{
    return EnumerateNames(cx, wrapper, 0, props);
}

template <typename Base>
bool
XrayWrapper<Base>::fix(JSContext *cx, JSObject *proxy, js::Value *vp)
{
    vp->setUndefined();
    return true;
}

template <typename Base>
bool
XrayWrapper<Base>::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                       js::Value *vp)
{
    // Skip our Base if it isn't already ProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return ProxyHandler::get(cx, wrapper, wrapper, id, vp);
}

template <typename Base>
bool
XrayWrapper<Base>::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                       bool strict, js::Value *vp)
{
    // Skip our Base if it isn't already ProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return ProxyHandler::set(cx, wrapper, wrapper, id, strict, vp);
}

template <typename Base>
bool
XrayWrapper<Base>::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Skip our Base if it isn't already ProxyHandler.
    return ProxyHandler::has(cx, wrapper, id, bp);
}

template <typename Base>
bool
XrayWrapper<Base>::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Skip our Base if it isn't already ProxyHandler.
    return ProxyHandler::hasOwn(cx, wrapper, id, bp);
}

template <typename Base>
bool
XrayWrapper<Base>::keys(JSContext *cx, JSObject *wrapper, JS::AutoIdVector &props)
{
    // Skip our Base if it isn't already ProxyHandler.
    return ProxyHandler::keys(cx, wrapper, props);
}

template <typename Base>
bool
XrayWrapper<Base>::iterate(JSContext *cx, JSObject *wrapper, unsigned flags, js::Value *vp)
{
    // Skip our Base if it isn't already ProxyHandler.
    return ProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base>
bool
XrayWrapper<Base>::call(JSContext *cx, JSObject *wrapper, unsigned argc, js::Value *vp)
{
    JSObject *holder = GetHolder(wrapper);
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);

    // Run the resolve hook of the wrapped native.
    if (NATIVE_HAS_FLAG(wn, WantCall)) {
        XPCCallContext ccx(JS_CALLER, cx, wrapper, nsnull, JSID_VOID, argc,
                           vp + 2, vp);
        if (!ccx.IsValid())
            return false;
        bool ok = true;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->Call(wn, cx, wrapper,
                                                                   argc, vp + 2, vp, &ok);
        if (NS_FAILED(rv)) {
            if (ok)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }

    return true;
}

template <typename Base>
bool
XrayWrapper<Base>::construct(JSContext *cx, JSObject *wrapper, unsigned argc,
                             js::Value *argv, js::Value *rval)
{
    JSObject *holder = GetHolder(wrapper);
    XPCWrappedNative *wn = GetWrappedNativeFromHolder(holder);

    // Run the resolve hook of the wrapped native.
    if (NATIVE_HAS_FLAG(wn, WantConstruct)) {
        XPCCallContext ccx(JS_CALLER, cx, wrapper, nsnull, JSID_VOID, argc, argv, rval);
        if (!ccx.IsValid())
            return false;
        bool ok = true;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->Construct(wn, cx, wrapper,
                                                                        argc, argv, rval, &ok);
        if (NS_FAILED(rv)) {
            if (ok)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }

    return true;
}

template <typename Base>
JSObject *
XrayWrapper<Base>::createHolder(JSContext *cx, JSObject *wrappedNative, JSObject *parent)
{
    JSObject *holder = JS_NewObjectWithGivenProto(cx, &HolderClass, nsnull, parent);
    if (!holder)
        return nsnull;

    CompartmentPrivate *priv =
        (CompartmentPrivate *)JS_GetCompartmentPrivate(js::GetObjectCompartment(holder));
    JSObject *inner = JS_ObjectToInnerObject(cx, wrappedNative);
    XPCWrappedNative *wn = GetWrappedNative(inner);
    Value expando = ObjectOrNullValue(priv->LookupExpandoObject(wn));

    // A note about ownership: the holder has a direct pointer to the wrapped
    // native that we're wrapping. Normally, we'd have to AddRef the pointer
    // so that it doesn't have to be collected, but then we'd have to tell the
    // cycle collector. Fortunately for us, we know that the Xray wrapper
    // itself has a reference to the flat JS object which will hold the
    // wrapped native alive. Furthermore, the reachability of that object and
    // the associated holder are exactly the same, so we can use that for our
    // strong reference.
    JS_ASSERT(IS_WN_WRAPPER(wrappedNative) ||
              js::GetObjectClass(wrappedNative)->ext.innerObject);
    js::SetReservedSlot(holder, JSSLOT_WN, PrivateValue(wn));
    js::SetReservedSlot(holder, JSSLOT_RESOLVING, PrivateValue(NULL));
    js::SetReservedSlot(holder, JSSLOT_EXPANDO, expando);
    return holder;
}

XrayProxy::XrayProxy(unsigned flags)
  : XrayWrapper<CrossCompartmentWrapper>(flags)
{
}

XrayProxy::~XrayProxy()
{
}

// The 'holder' here isn't actually of [[Class]] HolderClass like those used by
// XrayWrapper. Instead, it's a funny hybrid of the 'expando' and 'holder'
// properties. However, we store it in the same slot. Exercise caution.
static JSObject *
GetHolderObject(JSContext *cx, JSObject *wrapper, bool createHolder = true)
{
    if (!js::GetProxyExtra(wrapper, 0).isUndefined())
        return &js::GetProxyExtra(wrapper, 0).toObject();

    JSObject *obj = JS_NewObjectWithGivenProto(cx, nsnull, nsnull,
                                               JS_GetGlobalForObject(cx, wrapper));
    if (!obj)
        return nsnull;
    js::SetProxyExtra(wrapper, 0, ObjectValue(*obj));
    return obj;
}

bool
XrayProxy::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                 bool set, js::PropertyDescriptor *desc)
{
    JSObject *holder = GetHolderObject(cx, wrapper);
    if (!holder)
        return false;

    bool status;
    Wrapper::Action action = set ? Wrapper::SET : Wrapper::GET;
    desc->obj = NULL; // default value
    if (!this->enter(cx, wrapper, id, action, &status))
        return status;

    AutoLeaveHelper<CrossCompartmentWrapper> helper(*this, cx, wrapper);

    JSObject *obj = &js::GetProxyPrivate(wrapper).toObject();
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        {
            JSAutoEnterCompartment ac;
            if (!ac.enter(cx, obj))
                return false;
            if (!JS_GetPropertyDescriptorById(cx, obj, id,
                                              (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                              desc))
                return false;
        }

        if (desc->obj)
            desc->obj = wrapper;
        return JS_WrapPropertyDescriptor(cx, desc);
    }

    // We don't want to cache own properties on our holder. So we first do this
    // call, and return if we find it (without caching). If we don't find it,
    // we check the cache and do a full resolve (caching any result).
    if (!js::GetProxyHandler(obj)->getOwnPropertyDescriptor(cx, wrapper, id, set, desc))
        return false;
    if (desc->obj) {
        desc->obj = wrapper;
        return true;
    }

    // Now that we're sure this isn't an own property, look up cached non-own
    // properties before calling all the way through.
    if (!JS_GetPropertyDescriptorById(cx, holder, id, JSRESOLVE_QUALIFIED, desc))
        return false;
    if (desc->obj) {
        desc->obj = wrapper;
        return true;
    }

    // Nothing in the cache. Call through, and cache the result.
    if (!js::GetProxyHandler(obj)->getPropertyDescriptor(cx, wrapper, id, set, desc))
        return false;

    if (!desc->obj) {
        if (id != nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING))
            return true;

        JSFunction *toString = JS_NewFunction(cx, XrayToString, 0, 0, holder, "toString");
        if (!toString)
            return false;

        desc->attrs = 0;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->shortid = 0;
        desc->value.setObject(*JS_GetFunctionObject(toString));
    }

    desc->obj = wrapper;

    return JS_DefinePropertyById(cx, holder, id, desc->value, desc->getter, desc->setter,
                                 desc->attrs);
}

bool
XrayProxy::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                    bool set, js::PropertyDescriptor *desc)
{
    JSObject *holder = GetHolderObject(cx, wrapper);
    if (!holder)
        return false;

    bool status;
    Wrapper::Action action = set ? Wrapper::SET : Wrapper::GET;
    desc->obj = NULL; // default value
    if (!this->enter(cx, wrapper, id, action, &status))
        return status;

    AutoLeaveHelper<CrossCompartmentWrapper> helper(*this, cx, wrapper);

    JSObject *obj = &js::GetProxyPrivate(wrapper).toObject();
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        {
            JSAutoEnterCompartment ac;
            if (!ac.enter(cx, obj))
                return false;
            if (!JS_GetPropertyDescriptorById(cx, obj, id,
                                              (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                              desc))
                return false;
        }

        if (desc->obj)
            desc->obj = desc->obj == obj ? wrapper : NULL;
        return JS_WrapPropertyDescriptor(cx, desc);
    }

    if (!JS_GetPropertyDescriptorById(cx, holder, id, JSRESOLVE_QUALIFIED, desc))
        return false;
    if (desc->obj) {
        desc->obj = wrapper;
        return true;
    }

    if (!js::GetProxyHandler(obj)->getOwnPropertyDescriptor(cx, wrapper, id, set, desc))
        return false;

    // The 'not found' property descriptor has obj == NULL.
    if (desc->obj)
      desc->obj = wrapper;

    // Own properties don't get cached on the holder. Just return.
    return true;
}

bool
XrayProxy::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                          js::PropertyDescriptor *desc)
{
    JSObject *holder = GetHolderObject(cx, wrapper);
    if (!holder)
        return false;

    JSObject *obj = &js::GetProxyPrivate(wrapper).toObject();
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, obj) || !JS_WrapPropertyDescriptor(cx, desc))
            return false;

        return JS_DefinePropertyById(cx, obj, id, desc->value, desc->getter, desc->setter,
                                     desc->attrs);
    }

    PropertyDescriptor existing_desc;
    if (!getOwnPropertyDescriptor(cx, wrapper, id, true, &existing_desc))
        return false;

    if (existing_desc.obj && (existing_desc.attrs & JSPROP_PERMANENT))
        return true; // silently ignore attempt to overwrite native property

    return JS_DefinePropertyById(cx, holder, id, desc->value, desc->getter, desc->setter,
                                 desc->attrs);
}

static bool
EnumerateProxyNames(JSContext *cx, JSObject *wrapper, unsigned flags, JS::AutoIdVector &props)
{
    JSObject *obj = &js::GetProxyPrivate(wrapper).toObject();

    // Redirect access straight to the wrapper if we should be transparent.
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, obj))
            return false;

        return js::GetPropertyNames(cx, obj, flags, &props);
    }

    if (WrapperFactory::IsPartiallyTransparent(wrapper)) {
        JS_ReportError(cx, "Not allowed to enumerate cross origin objects");
        return false;
    }

    if (flags & (JSITER_OWNONLY | JSITER_HIDDEN))
        return js::GetProxyHandler(obj)->getOwnPropertyNames(cx, wrapper, props);

    return js::GetProxyHandler(obj)->enumerate(cx, wrapper, props);
}

bool
XrayProxy::getOwnPropertyNames(JSContext *cx, JSObject *wrapper, JS::AutoIdVector &props)
{
    return EnumerateProxyNames(cx, wrapper, JSITER_OWNONLY | JSITER_HIDDEN, props);
}

bool
XrayProxy::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    JSObject *obj = &js::GetProxyPrivate(wrapper).toObject();
    if (XrayUtils::IsTransparent(cx, wrapper)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, obj))
            return false;

        JSBool b;
        jsval v;
        if (!JS_DeletePropertyById2(cx, obj, id, &v) || !JS_ValueToBoolean(cx, v, &b))
            return false;

        *bp = b;

        return true;
    }

    if (!js::GetProxyHandler(obj)->delete_(cx, wrapper, id, bp))
        return false;

    JSObject *holder;
    if (*bp && (holder = GetHolderObject(cx, wrapper, false)))
        JS_DeletePropertyById(cx, holder, id);

    return true;
}

bool
XrayProxy::enumerate(JSContext *cx, JSObject *wrapper, JS::AutoIdVector &props)
{
    return EnumerateProxyNames(cx, wrapper, 0, props);
}

XrayProxy
XrayProxy::singleton(0);


#define XRAY XrayWrapper<CrossCompartmentSecurityWrapper>
template <> XRAY XRAY::singleton(0);
template class XRAY;
#undef XRAY

#define XRAY XrayWrapper<SameCompartmentSecurityWrapper>
template <> XRAY XRAY::singleton(0);
template class XRAY;
#undef XRAY

#define XRAY XrayWrapper<CrossCompartmentWrapper>
template <> XRAY XRAY::singleton(0);
template class XRAY;
#undef XRAY

}
