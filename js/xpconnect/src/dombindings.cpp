/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "dombindings.h"
#include "xpcpublic.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "XPCWrapper.h"
#include "WrapperFactory.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/BindingUtils.h"

#include "jsapi.h"
#include "jsatom.h"

using namespace JS;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {
namespace binding {

enum {
    JSPROXYSLOT_EXPANDO = 0
};

static jsid s_prototype_id = JSID_VOID;

static jsid s_length_id = JSID_VOID;

static jsid s_iterator_id = JSID_VOID;

static jsid s_VOID_id = JSID_VOID;

bool
DefineStaticJSVal(JSContext *cx, jsid &id, const char *string)
{
    if (JSString *str = ::JS_InternString(cx, string)) {
        id = INTERNED_STRING_TO_JSID(cx, str);
        return true;
    }
    return false;
}

#define SET_JSID_TO_STRING(_cx, _string)                                      \
    DefineStaticJSVal(_cx, s_##_string##_id, #_string)

bool
DefineStaticJSVals(JSContext *cx)
{
    JSAutoRequest ar(cx);

    return SET_JSID_TO_STRING(cx, prototype) &&
           SET_JSID_TO_STRING(cx, length) &&
           SET_JSID_TO_STRING(cx, iterator) &&
           DefinePropertyStaticJSVals(cx);
}


int HandlerFamily;

struct SetListBaseInformation
{
    SetListBaseInformation() {
        js::SetListBaseInformation((void*) &HandlerFamily, js::JSSLOT_PROXY_EXTRA + JSPROXYSLOT_EXPANDO);
    }
} gSetListBaseInformation;

JSBool
Throw(JSContext *cx, nsresult rv)
{
    XPCThrower::Throw(rv, cx);
    return false;
}

template<class T>
static bool
Wrap(JSContext *cx, JSObject *scope, T *p, nsWrapperCache *cache, jsval *vp)
{
    if (xpc_FastGetCachedWrapper(cache, scope, vp))
        return true;
    qsObjectHelper helper(p, cache);
    return XPCOMObjectToJsval(cx, scope, helper, NULL, true, vp);
}

template<class T>
static inline bool
Wrap(JSContext *cx, JSObject *scope, T *p, jsval *vp)
{
    return Wrap(cx, scope, p, GetWrapperCache(p), vp);
}

template<>
inline bool
Wrap(JSContext *cx, JSObject *scope, NoType *p, jsval *vp)
{
    NS_RUNTIMEABORT("We try to wrap the result from calling a noop?");
    return false;
}

template<class T>
inline bool
Wrap(JSContext *cx, JSObject *scope, nsCOMPtr<T> &p, jsval *vp)
{
    return Wrap(cx, scope, p.get(), vp);
}

static inline bool
Wrap(JSContext *cx, JSObject *scope, nsISupportsResult &result, jsval *vp)
{
    return Wrap(cx, scope, result.mResult, result.mCache, vp);
}

static inline bool
Wrap(JSContext *cx, JSObject *scope, nsString &result, jsval *vp)
{
    return xpc::StringToJsval(cx, result, vp);
}

template<class T>
bool
Unwrap(JSContext *cx, jsval v, T **ppArg, nsISupports **ppArgRef, jsval *vp)
{
    nsresult rv = xpc_qsUnwrapArg(cx, v, ppArg, ppArgRef, vp);
    if (NS_FAILED(rv))
        return Throw(cx, rv);
    return true;
}

template<>
bool
Unwrap(JSContext *cx, jsval v, NoType **ppArg, nsISupports **ppArgRef, jsval *vp)
{
    NS_RUNTIMEABORT("We try to unwrap an argument for a noop?");
    return false;
}

template<class LC>
typename ListBase<LC>::Properties ListBase<LC>::sProtoProperties[] = {
    { s_VOID_id, NULL, NULL }
};
template<class LC>
size_t ListBase<LC>::sProtoPropertiesCount = 0;

template<class LC>
typename ListBase<LC>::Methods ListBase<LC>::sProtoMethods[] = {
    { s_VOID_id, NULL, 0 }
};
template<class LC>
size_t ListBase<LC>::sProtoMethodsCount = 0;

template<class LC>
ListBase<LC> ListBase<LC>::instance;

bool
DefineConstructor(JSContext *cx, JSObject *obj, DefineInterface aDefine, nsresult *aResult)
{
    bool enabled;
    bool defined = aDefine(cx, obj, &enabled);
    NS_ASSERTION(!defined || enabled,
                 "We defined a constructor but the new bindings are disabled?");
    *aResult = defined ? NS_OK : NS_ERROR_FAILURE;
    return enabled;
}

template<class LC>
typename ListBase<LC>::ListType*
ListBase<LC>::getNative(JSObject *obj)
{
    return static_cast<ListType*>(js::GetProxyPrivate(obj).toPrivate());
}

template<class LC>
typename ListBase<LC>::ListType*
ListBase<LC>::getListObject(JSObject *obj)
{
    if (xpc::WrapperFactory::IsXrayWrapper(obj))
        obj = js::UnwrapObject(obj);
    JS_ASSERT(objIsList(obj));
    return getNative(obj);
}

static JSBool
UnwrapSecurityWrapper(JSContext *cx, JSObject *obj, JSObject *callee, JSObject **unwrapped)
{
    JS_ASSERT(XPCWrapper::IsSecurityWrapper(obj));

    if (callee && JS_GetGlobalForObject(cx, obj) == JS_GetGlobalForObject(cx, callee)) {
        *unwrapped = js::UnwrapObject(obj);
    } else {
        *unwrapped = XPCWrapper::Unwrap(cx, obj);
        if (!*unwrapped)
            return Throw(cx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
    }
    return true;
}

template<class LC>
bool
ListBase<LC>::instanceIsListObject(JSContext *cx, JSObject *obj, JSObject *callee)
{
    if (XPCWrapper::IsSecurityWrapper(obj) && !UnwrapSecurityWrapper(cx, obj, callee, &obj))
        return false;

    if (!objIsList(obj)) {
        // FIXME: Throw a proper DOM exception.
        JS_ReportError(cx, "type error: wrong object");
        return false;
    }
    return true;
}

template<class LC>
JSBool
ListBase<LC>::length_getter(JSContext *cx, JSHandleObject obj, JSHandleId id, JSMutableHandleValue vp)
{
    if (!instanceIsListObject(cx, obj, NULL))
        return false;
    PRUint32 length;
    getListObject(obj)->GetLength(&length);
    JS_ASSERT(int32_t(length) >= 0);
    vp.set(UINT_TO_JSVAL(length));
    return true;
}

template<class LC>
bool
ListBase<LC>::getItemAt(ListType *list, uint32_t i, IndexGetterType &item)
{
    JS_STATIC_ASSERT(!hasIndexGetter);
    return false;
}

template<class LC>
bool
ListBase<LC>::setItemAt(JSContext *cx, ListType *list, uint32_t i, IndexSetterType item)
{
    JS_STATIC_ASSERT(!hasIndexSetter);
    return false;
}

template<class LC>
bool
ListBase<LC>::getNamedItem(ListType *list, const nsAString& aName, NameGetterType &item)
{
    JS_STATIC_ASSERT(!hasNameGetter);
    return false;
}

template<class LC>
bool
ListBase<LC>::setNamedItem(JSContext *cx, ListType *list, const nsAString& aName,
                           NameSetterType item)
{
    JS_STATIC_ASSERT(!hasNameSetter);
    return false;
}

template<class LC>
bool
ListBase<LC>::namedItem(JSContext *cx, JSObject *obj, jsval *name, NameGetterType &result,
                        bool *hasResult)
{
    xpc_qsDOMString nameString(cx, *name, name,
                               xpc_qsDOMString::eDefaultNullBehavior,
                               xpc_qsDOMString::eDefaultUndefinedBehavior);
    if (!nameString.IsValid())
        return false;
    *hasResult = getNamedItem(getListObject(obj), nameString, result);
    return true;
}

JSBool
interface_hasInstance(JSContext *cx, JSHandleObject obj, const JS::Value *vp, JSBool *bp)
{
    if (vp->isObject()) {
        jsval prototype;
        if (!JS_GetPropertyById(cx, obj, s_prototype_id, &prototype) ||
            JSVAL_IS_PRIMITIVE(prototype)) {
            JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                         JSMSG_THROW_TYPE_ERROR);
            return false;
        }

        JSObject *other = &vp->toObject();
        if (instanceIsProxy(other)) {
            ProxyHandler *handler = static_cast<ProxyHandler*>(js::GetProxyHandler(other));
            if (handler->isInstanceOf(JSVAL_TO_OBJECT(prototype))) {
                *bp = true;
            } else {
                JSObject *protoObj = JSVAL_TO_OBJECT(prototype);
                JSObject *proto = other;
                while ((proto = JS_GetPrototype(proto))) {
                    if (proto == protoObj) {
                        *bp = true;
                        return true;
                    }
                }
                *bp = false;
            }

            return true;
        }
    }

    *bp = false;
    return true;
}

js::Class sInterfacePrototypeClass = {
    "Object",
    JSCLASS_HAS_RESERVED_SLOTS(0),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template<class LC>
JSObject *
ListBase<LC>::getPrototype(JSContext *cx, JSObject *receiver, bool *enabled)
{
    *enabled = true;

    XPCWrappedNativeScope *scope =
        XPCWrappedNativeScope::FindInJSObjectScope(cx, receiver);
    if (!scope)
        return NULL;

    return getPrototype(cx, scope, receiver);
}

template<class LC>
JSObject *
ListBase<LC>::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope,
                           JSObject *receiver)
{
    nsDataHashtable<nsDepCharHashKey, JSObject*> &cache =
        scope->GetCachedDOMPrototypes();

    JSObject *interfacePrototype;
    if (cache.IsInitialized()) {
        if (cache.Get(sInterfaceClass.name, &interfacePrototype)) {
            xpc_UnmarkGrayObject(interfacePrototype);
            return interfacePrototype;
        }
    } else {
        cache.Init();
    }

    JSObject* proto = Base::getPrototype(cx, scope, receiver);
    if (!proto)
        return NULL;

    JSObject *global = scope->GetGlobalJSObject();
    interfacePrototype = JS_NewObject(cx, Jsvalify(&sInterfacePrototypeClass), proto, global);
    if (!interfacePrototype)
        return NULL;

    for (size_t n = 0; n < sProtoPropertiesCount; ++n) {
        JS_ASSERT(sProtoProperties[n].getter);
        jsid id = sProtoProperties[n].id;
        unsigned attrs = JSPROP_ENUMERATE | JSPROP_SHARED;
        if (!sProtoProperties[n].setter)
            attrs |= JSPROP_READONLY;
        if (!JS_DefinePropertyById(cx, interfacePrototype, id, JSVAL_VOID,
                                   sProtoProperties[n].getter, sProtoProperties[n].setter, attrs))
            return NULL;
    }

    for (size_t n = 0; n < sProtoMethodsCount; ++n) {
        jsid id = sProtoMethods[n].id;
        JSFunction *fun = JS_NewFunctionById(cx, sProtoMethods[n].native, sProtoMethods[n].nargs,
                                             0, js::GetObjectParent(interfacePrototype), id);
        if (!fun)
            return NULL;
        JSObject *funobj = JS_GetFunctionObject(fun);
        if (!JS_DefinePropertyById(cx, interfacePrototype, id, OBJECT_TO_JSVAL(funobj),
                                   NULL, NULL, JSPROP_ENUMERATE))
            return NULL;
    }

    JSObject *interface = JS_NewObject(cx, Jsvalify(&sInterfaceClass), NULL, global);
    if (!interface)
        return NULL;

    if (!JS_LinkConstructorAndPrototype(cx, interface, interfacePrototype))
        return NULL;

    if (!JS_DefineProperty(cx, receiver, sInterfaceClass.name, OBJECT_TO_JSVAL(interface), NULL,
                           NULL, 0))
        return NULL;

    if (!cache.Put(sInterfaceClass.name, interfacePrototype, fallible_t()))
        return NULL;

    return interfacePrototype;
}

template<class LC>
JSObject *
ListBase<LC>::create(JSContext *cx, JSObject *scope, ListType *aList,
                     nsWrapperCache* aWrapperCache, bool *triedToWrap)
{
    *triedToWrap = true;

    JSObject *parent = WrapNativeParent(cx, scope, aList->GetParentObject());
    if (!parent)
        return NULL;

    JSObject *global = js::GetGlobalForObjectCrossCompartment(parent);

    JSAutoEnterCompartment ac;
    if (global != scope) {
        if (!ac.enter(cx, global))
            return NULL;
    }

    JSObject *proto = getPrototype(cx, global, triedToWrap);
    if (!proto && !*triedToWrap)
        aWrapperCache->ClearIsDOMBinding();
    if (!proto)
        return NULL;
    JSObject *obj = NewProxyObject(cx, &ListBase<LC>::instance,
                                   PrivateValue(aList), proto, parent);
    if (!obj)
        return NULL;

    NS_ADDREF(aList);

    aWrapperCache->SetWrapper(obj);

    return obj;
}

static JSObject *
getExpandoObject(JSObject *obj)
{
    NS_ASSERTION(instanceIsProxy(obj), "expected a DOM proxy object");
    Value v = js::GetProxyExtra(obj, JSPROXYSLOT_EXPANDO);
    return v.isUndefined() ? NULL : v.toObjectOrNull();
}

static int32_t
IdToInt32(JSContext *cx, jsid id)
{
    JSAutoRequest ar(cx);

    jsval idval;
    double array_index;
    int32_t i;
    if (!::JS_IdToValue(cx, id, &idval) ||
        !::JS_ValueToNumber(cx, idval, &array_index) ||
        !::JS_DoubleIsInt32(array_index, &i)) {
        return -1;
    }

    return i;
}

static inline int32_t
GetArrayIndexFromId(JSContext *cx, jsid id)
{
    if (NS_LIKELY(JSID_IS_INT(id)))
        return JSID_TO_INT(id);
    if (NS_LIKELY(id == s_length_id))
        return -1;
    if (NS_LIKELY(JSID_IS_ATOM(id))) {
        JSAtom *atom = JSID_TO_ATOM(id);
        jschar s = *js::GetAtomChars(atom);
        if (NS_LIKELY((unsigned)s >= 'a' && (unsigned)s <= 'z'))
            return -1;

        uint32_t i;
        JSLinearString *str = js::AtomToLinearString(JSID_TO_ATOM(id));
        return js::StringIsArrayIndex(str, &i) ? i : -1;
    }
    return IdToInt32(cx, id);
}

static void
FillPropertyDescriptor(JSPropertyDescriptor *desc, JSObject *obj, jsval v, bool readonly)
{
    desc->obj = obj;
    desc->value = v;
    desc->attrs = (readonly ? JSPROP_READONLY : 0) | JSPROP_ENUMERATE;
    desc->getter = NULL;
    desc->setter = NULL;
    desc->shortid = 0;
}

template<class LC>
bool
ListBase<LC>::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                       JSPropertyDescriptor *desc)
{
    if (set) {
        if (hasIndexSetter) {
            int32_t index = GetArrayIndexFromId(cx, id);
            if (index >= 0) {
                FillPropertyDescriptor(desc, proxy, JSVAL_VOID, false);
                return true;
            }
        }

        if (hasNameSetter && JSID_IS_STRING(id)) {
            FillPropertyDescriptor(desc, proxy, JSVAL_VOID, false);
            return true;
        }
    } else {
        if (hasIndexGetter) {
            int32_t index = GetArrayIndexFromId(cx, id);
            if (index >= 0) {
                IndexGetterType result;
                if (!getItemAt(getListObject(proxy), PRUint32(index), result))
                    return true;

                jsval v;
                if (!Wrap(cx, proxy, result, &v))
                    return false;
                FillPropertyDescriptor(desc, proxy, v, !hasIndexSetter);
                return true;
            }
        }
    }

    JSObject *expando;
    if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = getExpandoObject(proxy))) {
        unsigned flags = (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED;
        if (!JS_GetPropertyDescriptorById(cx, expando, id, flags, desc))
            return false;
        if (desc->obj) {
            // Pretend the property lives on the wrapper.
            desc->obj = proxy;
            return true;
        }
    }

    if (hasNameGetter && !set && JSID_IS_STRING(id) && !hasPropertyOnPrototype(cx, proxy, id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        bool hasResult;
        NameGetterType result;
        if (!namedItem(cx, proxy, &name, result, &hasResult))
            return false;
        if (hasResult) {
            jsval v;
            if (!Wrap(cx, proxy, result, &v))
                return false;
            FillPropertyDescriptor(desc, proxy, v, !hasNameSetter);
            return true;
        }
    }

    desc->obj = NULL;
    return true;
}

template<class LC>
bool
ListBase<LC>::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                    JSPropertyDescriptor *desc)
{
    if (!getOwnPropertyDescriptor(cx, proxy, id, set, desc))
        return false;
    if (desc->obj)
        return true;
    if (xpc::WrapperFactory::IsXrayWrapper(proxy))
        return resolveNativeName(cx, proxy, id, desc);
    JSObject *proto = js::GetObjectProto(proxy);
    if (!proto) {
        desc->obj = NULL;
        return true;
    }
    return JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, desc);
}

JSClass ExpandoClass = {
    "DOM proxy binding expando object",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template<class LC>
JSObject *
ListBase<LC>::ensureExpandoObject(JSContext *cx, JSObject *obj)
{
    NS_ASSERTION(instanceIsProxy(obj), "expected a DOM proxy object");
    JSObject *expando = getExpandoObject(obj);
    if (!expando) {
        expando = JS_NewObjectWithGivenProto(cx, &ExpandoClass, nullptr,
                                             js::GetObjectParent(obj));
        if (!expando)
            return NULL;

        JSCompartment *compartment = js::GetObjectCompartment(obj);
        xpc::CompartmentPrivate *priv = xpc::GetCompartmentPrivate(compartment);
        if (!priv->RegisterDOMExpandoObject(obj))
            return NULL;

        nsWrapperCache* cache;
        CallQueryInterface(getListObject(obj), &cache);
        cache->SetPreservingWrapper(true);

        js::SetProxyExtra(obj, JSPROXYSLOT_EXPANDO, ObjectValue(*expando));
        JS_SetPrivate(expando, js::GetProxyPrivate(obj).toPrivate());
    }
    return expando;
}

template<class LC>
bool
ListBase<LC>::defineProperty(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc)
{
    if (hasIndexSetter) {
        int32_t index = GetArrayIndexFromId(cx, id);
        if (index >= 0) {
            nsCOMPtr<nsISupports> ref;
            IndexSetterType value;
            jsval v;
            return Unwrap(cx, desc->value, &value, getter_AddRefs(ref), &v) &&
                   setItemAt(cx, getListObject(proxy), index, value);
        }
    }

    if (hasNameSetter && JSID_IS_STRING(id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        xpc_qsDOMString nameString(cx, name, &name,
                                   xpc_qsDOMString::eDefaultNullBehavior,
                                   xpc_qsDOMString::eDefaultUndefinedBehavior);
        if (!nameString.IsValid())
            return false;

        nsCOMPtr<nsISupports> ref;
        NameSetterType value;
        jsval v;
        if (!Unwrap(cx, desc->value, &value, getter_AddRefs(ref), &v))
            return false;
        return setNamedItem(cx, getListObject(proxy), nameString, value);
    }

    if (xpc::WrapperFactory::IsXrayWrapper(proxy))
        return true;

    JSObject *expando = ensureExpandoObject(cx, proxy);
    if (!expando)
        return false;

    return JS_DefinePropertyById(cx, expando, id, desc->value, desc->getter, desc->setter,
                                 desc->attrs);
}

template<class LC>
bool
ListBase<LC>::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    PRUint32 length;
    getListObject(proxy)->GetLength(&length);
    JS_ASSERT(int32_t(length) >= 0);
    for (int32_t i = 0; i < int32_t(length); ++i) {
        if (!props.append(INT_TO_JSID(i)))
            return false;
    }

    JSObject *expando;
    if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = getExpandoObject(proxy)) &&
        !js::GetPropertyNames(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN, &props))
        return false;

    // FIXME: Add named items
    return true;
}

template<class LC>
bool
ListBase<LC>::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSBool b = true;

    JSObject *expando;
    if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = getExpandoObject(proxy))) {
        jsval v;
        if (!JS_DeletePropertyById2(cx, expando, id, &v) ||
            !JS_ValueToBoolean(cx, v, &b)) {
            return false;
        }
    }

    *bp = !!b;
    return true;
}

template<class LC>
bool
ListBase<LC>::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JSObject *proto = JS_GetPrototype(proxy);
    return getOwnPropertyNames(cx, proxy, props) &&
           (!proto || js::GetPropertyNames(cx, proto, 0, &props));
}

template<class LC>
bool
ListBase<LC>::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    if (hasIndexGetter) {
        int32_t index = GetArrayIndexFromId(cx, id);
        if (index >= 0) {
            IndexGetterType result;
            *bp = getItemAt(getListObject(proxy), PRUint32(index), result);
            return true;
        }
    }

    JSObject *expando = getExpandoObject(proxy);
    if (expando) {
        JSBool b = true;
        JSBool ok = JS_HasPropertyById(cx, expando, id, &b);
        *bp = !!b;
        if (!ok || *bp)
            return ok;
    }

    if (hasNameGetter && JSID_IS_STRING(id) && !hasPropertyOnPrototype(cx, proxy, id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        NameGetterType result;
        return namedItem(cx, proxy, &name, result, bp);
    }

    *bp = false;
    return true;
}

template<class LC>
bool
ListBase<LC>::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    if (!hasOwn(cx, proxy, id, bp))
        return false;
    // We have the property ourselves; no need to worry about our
    // prototype chain.
    if (*bp)
        return true;

    // OK, now we have to look at the proto
    JSObject *proto = js::GetObjectProto(proxy);
    if (!proto)
        return true;

    JSBool protoHasProp;
    bool ok = JS_HasPropertyById(cx, proto, id, &protoHasProp);
    if (ok)
        *bp = protoHasProp;
    return ok;
}

template<class LC>
bool
ListBase<LC>::resolveNativeName(JSContext *cx, JSObject *proxy, jsid id, JSPropertyDescriptor *desc)
{
    JS_ASSERT(xpc::WrapperFactory::IsXrayWrapper(proxy));

    for (size_t n = 0; n < sProtoPropertiesCount; ++n) {
        if (id == sProtoProperties[n].id) {
            desc->attrs = JSPROP_ENUMERATE | JSPROP_SHARED;
            if (!sProtoProperties[n].setter)
                desc->attrs |= JSPROP_READONLY;
            desc->obj = proxy;
            desc->setter = sProtoProperties[n].setter;
            desc->getter = sProtoProperties[n].getter;
            return true;
        }
    }

    for (size_t n = 0; n < sProtoMethodsCount; ++n) {
        if (id == sProtoMethods[n].id) {
            JSFunction *fun = JS_NewFunctionById(cx, sProtoMethods[n].native,
                                                 sProtoMethods[n].nargs, 0, proxy, id);
            if (!fun)
                return false;
            JSObject *funobj = JS_GetFunctionObject(fun);
            desc->value.setObject(*funobj);
            desc->attrs = JSPROP_ENUMERATE;
            desc->obj = proxy;
            desc->setter = nullptr;
            desc->getter = nullptr;
            return true;
        }
    }

    return Base::resolveNativeName(cx, proxy, id, desc);
}

template<class LC>
bool
ListBase<LC>::getPropertyOnPrototype(JSContext *cx, JSObject *proxy, jsid id, bool *found,
                                     JS::Value *vp)
{
    JSObject *proto = js::GetObjectProto(proxy);
    if (!proto)
        return true;

    JSBool hasProp;
    if (!JS_HasPropertyById(cx, proto, id, &hasProp))
        return false;

    *found = hasProp;
    if (!hasProp || !vp)
        return true;

    return JS_ForwardGetPropertyTo(cx, proto, id, proxy, vp);
}

template<class LC>
bool
ListBase<LC>::hasPropertyOnPrototype(JSContext *cx, JSObject *proxy, jsid id)
{
    JSAutoEnterCompartment ac;
    if (xpc::WrapperFactory::IsXrayWrapper(proxy)) {
        proxy = js::UnwrapObject(proxy);
        if (!ac.enter(cx, proxy))
            return false;
    }
    JS_ASSERT(objIsList(proxy));

    bool found = false;
    // We ignore an error from getPropertyOnPrototype.
    return !getPropertyOnPrototype(cx, proxy, id, &found, NULL) || found;
}

template<class LC>
bool
ListBase<LC>::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    NS_ASSERTION(!xpc::WrapperFactory::IsXrayWrapper(proxy),
                 "Should not have a XrayWrapper here");

    bool getFromExpandoObject = true;

    if (hasIndexGetter) {
        int32_t index = GetArrayIndexFromId(cx, id);
        if (index >= 0) {
            IndexGetterType result;
            if (getItemAt(getListObject(proxy), PRUint32(index), result))
                return Wrap(cx, proxy, result, vp);

            // Even if we don't have this index, we don't forward the
            // get on to our expando object.
            getFromExpandoObject = false;
        }
    }

    if (getFromExpandoObject) {
        JSObject *expando = getExpandoObject(proxy);
        if (expando) {
            JSBool hasProp;
            if (!JS_HasPropertyById(cx, expando, id, &hasProp))
                return false;

            if (hasProp)
                return JS_GetPropertyById(cx, expando, id, vp);
        }
    }

    bool found = false;
    if (!getPropertyOnPrototype(cx, proxy, id, &found, vp))
        return false;

    if (found)
        return true;

    if (hasNameGetter && JSID_IS_STRING(id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        bool hasResult;
        NameGetterType result;
        if (!namedItem(cx, proxy, &name, result, &hasResult))
            return false;
        if (hasResult)
            return Wrap(cx, proxy, result, vp);
    }

    vp->setUndefined();
    return true;
}

template<class LC>
bool
ListBase<LC>::getElementIfPresent(JSContext *cx, JSObject *proxy, JSObject *receiver,
                                  uint32_t index, Value *vp, bool *present)
{
    NS_ASSERTION(!xpc::WrapperFactory::IsXrayWrapper(proxy),
                 "Should not have a XrayWrapper here");

    if (hasIndexGetter) {
        IndexGetterType result;
        *present = getItemAt(getListObject(proxy), index, result);
        if (*present)
            return Wrap(cx, proxy, result, vp);
    }

    jsid id;
    if (!JS_IndexToId(cx, index, &id))
        return false;

    // if hasIndexGetter, we skip the expando object
    if (!hasIndexGetter) {
        JSObject *expando = getExpandoObject(proxy);
        if (expando) {
            JSBool isPresent;
            if (!JS_GetElementIfPresent(cx, expando, index, expando, vp, &isPresent))
                return false;
            if (isPresent) {
                *present = true;
                return true;
            }
        }
    }

    // No need to worry about name getters here, so just check the proto.

    JSObject *proto = js::GetObjectProto(proxy);
    if (proto) {
        JSBool isPresent;
        if (!JS_GetElementIfPresent(cx, proto, index, proxy, vp, &isPresent))
            return false;
        *present = isPresent;
        return true;
    }

    *present = false;
    // Can't Debug_SetValueRangeToCrashOnTouch because it's not public
    return true;
}

template<class LC>
bool
ListBase<LC>::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                  Value *vp)
{
    return ProxyHandler::set(cx, proxy, proxy, id, strict, vp);
}

template<class LC>
bool
ListBase<LC>::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return ProxyHandler::keys(cx, proxy, props);
}

template<class LC>
bool
ListBase<LC>::iterate(JSContext *cx, JSObject *proxy, unsigned flags, Value *vp)
{
    return ProxyHandler::iterate(cx, proxy, flags, vp);
}

template<class LC>
bool
ListBase<LC>::hasInstance(JSContext *cx, JSObject *proxy, const Value *vp, bool *bp)
{
    *bp = vp->isObject() && js::GetObjectClass(&vp->toObject()) == &sInterfaceClass;
    return true;
}

template<class LC>
JSString *
ListBase<LC>::obj_toString(JSContext *cx, JSObject *proxy)
{
    const char *clazz = sInterfaceClass.name;
    size_t nchars = 9 + strlen(clazz); /* 9 for "[object ]" */
    jschar *chars = (jschar *)JS_malloc(cx, (nchars + 1) * sizeof(jschar));
    if (!chars)
        return NULL;

    const char *prefix = "[object ";
    nchars = 0;
    while ((chars[nchars] = (jschar)*prefix) != 0)
        nchars++, prefix++;
    while ((chars[nchars] = (jschar)*clazz) != 0)
        nchars++, clazz++;
    chars[nchars++] = ']';
    chars[nchars] = 0;

    JSString *str = JS_NewUCString(cx, chars, nchars);
    if (!str)
        JS_free(cx, chars);
    return str;
}

template<class LC>
void
ListBase<LC>::finalize(JSFreeOp *fop, JSObject *proxy)
{
    ListType *list = getListObject(proxy);
    nsWrapperCache *cache;
    CallQueryInterface(list, &cache);
    if (cache) {
        cache->ClearWrapper();
    }
    XPCJSRuntime *rt = nsXPConnect::GetRuntimeInstance();
    if (rt) {
        rt->DeferredRelease(nativeToSupports(list));
    }
    else {
        NS_RELEASE(list);
    }
}


JSObject*
NoBase::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope, JSObject *receiver)
{
    // We need to pass the object prototype to JS_NewObject. If we pass NULL then the JS engine
    // will look up a prototype on the global by using the class' name and we'll recurse into
    // getPrototype.
    return JS_GetObjectPrototype(cx, receiver);
}


}
}
}
#include "dombindings_gen.cpp"
