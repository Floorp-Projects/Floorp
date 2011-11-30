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

#include "mozilla/Util.h"

#include "dombindings.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "XPCWrapper.h"
#include "WrapperFactory.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "jsiter.h"
#include "nsWrapperCacheInlines.h"

using namespace js;

namespace mozilla {
namespace dom {
namespace binding {


static jsid s_constructor_id = JSID_VOID;
static jsid s_prototype_id = JSID_VOID;

static jsid s_length_id = JSID_VOID;

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

    return SET_JSID_TO_STRING(cx, constructor) &&
           SET_JSID_TO_STRING(cx, prototype) &&
           SET_JSID_TO_STRING(cx, length) &&
           DefinePropertyStaticJSVals(cx);
}


int HandlerFamily;


JSBool
Throw(JSContext *cx, nsresult rv)
{
    XPCThrower::Throw(rv, cx);
    return false;
}


// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
static bool
XPCOMObjectToJsval(JSContext *cx, JSObject *scope, xpcObjectHelper &helper,
                   bool allowNativeWrapper, jsval *rval)
{
    // XXX The OBJ_IS_NOT_GLOBAL here is not really right. In
    // fact, this code is depending on the fact that the
    // global object will not have been collected, and
    // therefore this NativeInterface2JSObject will not end up
    // creating a new XPCNativeScriptableShared.

    XPCLazyCallContext lccx(JS_CALLER, cx, scope);

    nsresult rv;
    if (!XPCConvert::NativeInterface2JSObject(lccx, rval, NULL, helper, NULL, NULL,
                                              allowNativeWrapper, OBJ_IS_NOT_GLOBAL, &rv)) {
        // I can't tell if NativeInterface2JSObject throws JS exceptions
        // or not.  This is a sloppy stab at the right semantics; the
        // method really ought to be fixed to behave consistently.
        if (!JS_IsExceptionPending(cx))
            Throw(cx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
        return false;
    }

#ifdef DEBUG
    JSObject* jsobj = JSVAL_TO_OBJECT(*rval);
    if (jsobj && !js::GetObjectParent(jsobj))
        NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                     "Why did we recreate this wrapper?");
#endif

    return true;
}

template<class T>
static inline JSObject*
WrapNativeParent(JSContext *cx, JSObject *scope, T *p)
{
    if (!p)
        return NULL;

    nsWrapperCache *cache = GetWrapperCache(p);
    JSObject* obj;
    if (cache && (obj = cache->GetWrapper())) {
#ifdef DEBUG
        qsObjectHelper helper(p, cache);
        jsval debugVal;

        bool ok = XPCOMObjectToJsval(cx, scope, helper, false, &debugVal);
        NS_ASSERTION(ok && JSVAL_TO_OBJECT(debugVal) == obj,
                     "Unexpected object in nsWrapperCache");
#endif
        return obj;
    }

    qsObjectHelper helper(p, cache);
    jsval v;
    return XPCOMObjectToJsval(cx, scope, helper, false, &v) ? JSVAL_TO_OBJECT(v) : NULL;
}

template<class T>
static bool
Wrap(JSContext *cx, JSObject *scope, T *p, nsWrapperCache *cache, jsval *vp)
{
    if (xpc_FastGetCachedWrapper(cache, scope, vp))
        return true;
    qsObjectHelper helper(p, cache);
    return XPCOMObjectToJsval(cx, scope, helper, true, vp);
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
    return xpc_qsStringToJsval(cx, result, vp);
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
ListBase<LC> ListBase<LC>::instance;

bool
DefineConstructor(JSContext *cx, JSObject *obj, DefineInterface aDefine, nsresult *aResult)
{
    bool enabled;
    bool defined = !!aDefine(cx, XPCWrappedNativeScope::FindInJSObjectScope(cx, obj), &enabled);
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

template<class LC>
uint32
ListBase<LC>::getProtoShape(JSObject *obj)
{
    JS_ASSERT(objIsList(obj));
    return js::GetProxyExtra(obj, JSPROXYSLOT_PROTOSHAPE).toPrivateUint32();
}

template<class LC>
void
ListBase<LC>::setProtoShape(JSObject *obj, uint32 shape)
{
    JS_ASSERT(objIsList(obj));
    js::SetProxyExtra(obj, JSPROXYSLOT_PROTOSHAPE, PrivateUint32Value(shape));
}

template<class LC>
bool
ListBase<LC>::instanceIsListObject(JSContext *cx, JSObject *obj, JSObject *callee)
{
    if (XPCWrapper::IsSecurityWrapper(obj)) {
        if (callee && js::GetObjectGlobal(obj) == js::GetObjectGlobal(callee)) {
            obj = js::UnwrapObject(obj);
        } else {
            obj = XPCWrapper::Unwrap(cx, obj);
            if (!obj)
                return Throw(cx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
        }
    }

    if (!objIsList(obj)) {
        // FIXME: Throw a proper DOM exception.
        JS_ReportError(cx, "type error: wrong object");
        return false;
    }
    return true;
}

template<class LC>
JSBool
ListBase<LC>::length_getter(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!instanceIsListObject(cx, obj, NULL))
        return false;
    PRUint32 length;
    getListObject(obj)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    *vp = UINT_TO_JSVAL(length);
    return true;
}

template<class LC>
bool
ListBase<LC>::getItemAt(ListType *list, uint32 i, IndexGetterType &item)
{
    JS_STATIC_ASSERT(!hasIndexGetter);
    return false;
}

template<class LC>
bool
ListBase<LC>::setItemAt(JSContext *cx, ListType *list, uint32 i, IndexSetterType item)
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
interface_hasInstance(JSContext *cx, JSObject *obj, const js::Value *vp, JSBool *bp)
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
                while ((proto = JS_GetPrototype(cx, proto))) {
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

template<class LC>
JSObject *
ListBase<LC>::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope, bool *enabled)
{
    if (!scope->NewDOMBindingsEnabled()) {
        *enabled = false;
        return NULL;
    }

    *enabled = true;

    return getPrototype(cx, scope);
}

template<class LC>
JSObject *
ListBase<LC>::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope)
{
    nsDataHashtable<nsDepCharHashKey, JSObject*> &cache =
        scope->GetCachedDOMPrototypes();

    JSObject *interfacePrototype;
    if (cache.IsInitialized()) {
        if (cache.Get(sInterfaceClass.name, &interfacePrototype))
            return interfacePrototype;
    } else if (!cache.Init()) {
        return NULL;
    }

    JSObject* proto = Base::getPrototype(cx, scope);
    if (!proto)
        return NULL;

    JSObject *global = scope->GetGlobalJSObject();
    interfacePrototype = JS_NewObject(cx, NULL, proto, global);
    if (!interfacePrototype)
        return NULL;

    for (size_t n = 0; n < ArrayLength(sProtoProperties); ++n) {
        JS_ASSERT(sProtoProperties[n].getter);
        jsid id = sProtoProperties[n].id;
        uintN attrs = JSPROP_ENUMERATE | JSPROP_SHARED;
        if (!sProtoProperties[n].setter)
            attrs |= JSPROP_READONLY;
        if (!JS_DefinePropertyById(cx, interfacePrototype, id, JSVAL_VOID,
                                   sProtoProperties[n].getter, sProtoProperties[n].setter, attrs))
            return NULL;
    }

    for (size_t n = 0; n < ArrayLength(sProtoMethods); ++n) {
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
    if (!interface ||
        !JS_DefinePropertyById(cx, interface, s_prototype_id, OBJECT_TO_JSVAL(interfacePrototype),
                               nsnull, nsnull, JSPROP_PERMANENT | JSPROP_READONLY))
        return NULL;

    if (!JS_DefinePropertyById(cx, interfacePrototype, s_constructor_id,
                               OBJECT_TO_JSVAL(interface), nsnull, nsnull, 0))
        return NULL;

    if (!JS_DefineProperty(cx, global, sInterfaceClass.name, OBJECT_TO_JSVAL(interface), NULL,
                           NULL, 0))
        return NULL;

    if (!cache.Put(sInterfaceClass.name, interfacePrototype))
        return NULL;

    return interfacePrototype;
}

template<class LC>
JSObject *
ListBase<LC>::create(JSContext *cx, XPCWrappedNativeScope *scope, ListType *aList,
                     nsWrapperCache* aWrapperCache, bool *triedToWrap)
{
    *triedToWrap = true;

    JSObject *parent = WrapNativeParent(cx, scope->GetGlobalJSObject(), aList->GetParentObject());
    if (!parent)
        return NULL;

    JSAutoEnterCompartment ac;
    if (js::GetObjectGlobal(parent) != scope->GetGlobalJSObject()) {
        if (!ac.enter(cx, parent))
            return NULL;

        scope = XPCWrappedNativeScope::FindInJSObjectScope(cx, parent);
    }

    JSObject *proto = getPrototype(cx, scope, triedToWrap);
    if (!proto && !*triedToWrap)
        aWrapperCache->ClearIsProxy();
    if (!proto)
        return NULL;
    JSObject *obj = NewProxyObject(cx, &ListBase<LC>::instance,
                                   PrivateValue(aList), proto, parent);
    if (!obj)
        return NULL;

    NS_ADDREF(aList);
    setProtoShape(obj, -1);

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

static int32
IdToInt32(JSContext *cx, jsid id)
{
    JSAutoRequest ar(cx);

    jsval idval;
    jsdouble array_index;
    jsint i;
    if (!::JS_IdToValue(cx, id, &idval) ||
        !::JS_ValueToNumber(cx, idval, &array_index) ||
        !::JS_DoubleIsInt32(array_index, &i)) {
        return -1;
    }

    return i;
}

static inline int32
GetArrayIndexFromId(JSContext *cx, jsid id)
{
    if (NS_LIKELY(JSID_IS_INT(id)))
        return JSID_TO_INT(id);
    if (NS_LIKELY(id == s_length_id))
        return -1;
    if (NS_LIKELY(JSID_IS_ATOM(id))) {
        JSAtom *atom = JSID_TO_ATOM(id);
        jschar s = *atom->chars();
        if (NS_LIKELY((unsigned)s >= 'a' && (unsigned)s <= 'z'))
            return -1;

        jsuint i;
        return js::StringIsArrayIndex(JSID_TO_ATOM(id), &i) ? i : -1;
    }
    return IdToInt32(cx, id);
}

static void
FillPropertyDescriptor(PropertyDescriptor *desc, JSObject *obj, jsval v, bool readonly)
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
                                       PropertyDescriptor *desc)
{
    if (set) {
        if (hasIndexSetter) {
            int32 index = GetArrayIndexFromId(cx, id);
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
            int32 index = GetArrayIndexFromId(cx, id);
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
        uintN flags = (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED;
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
                                    PropertyDescriptor *desc)
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
        expando = JS_NewObjectWithGivenProto(cx, &ExpandoClass, nsnull,
                                             js::GetObjectParent(obj));
        if (!expando)
            return NULL;

        JSCompartment *compartment = js::GetObjectCompartment(obj);
        xpc::CompartmentPrivate *priv =
            static_cast<xpc::CompartmentPrivate *>(js_GetCompartmentPrivate(compartment));
        if (!priv->RegisterDOMExpandoObject(expando))
            return NULL;

        js::SetProxyExtra(obj, JSPROXYSLOT_EXPANDO, ObjectValue(*expando));
        JS_SetPrivate(cx, expando, js::GetProxyPrivate(obj).toPrivate());
    }
    return expando;
}

template<class LC>
bool
ListBase<LC>::defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    if (hasIndexSetter) {
        int32 index = GetArrayIndexFromId(cx, id);
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
    JS_ASSERT(int32(length) >= 0);
    for (int32 i = 0; i < int32(length); ++i) {
        if (!props.append(INT_TO_JSID(i)))
            return false;
    }

    JSObject *expando;
    if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = getExpandoObject(proxy)) &&
        !GetPropertyNames(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN, &props))
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
    // FIXME: enumerate proto as well
    return getOwnPropertyNames(cx, proxy, props);
}

template<class LC>
bool
ListBase<LC>::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    vp->setUndefined();
    return true;
}

template<class LC>
bool
ListBase<LC>::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    if (hasIndexGetter) {
        int32 index = GetArrayIndexFromId(cx, id);
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
ListBase<LC>::shouldCacheProtoShape(JSContext *cx, JSObject *proto, bool *shouldCache)
{
    JSPropertyDescriptor desc;
    for (size_t n = 0; n < ArrayLength(sProtoProperties); ++n) {
        jsid id = sProtoProperties[n].id;
        if (!JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, &desc))
            return false;
        if (desc.obj != proto || desc.getter != sProtoProperties[n].getter ||
            desc.setter != sProtoProperties[n].setter) {
            *shouldCache = false;
            return true;
        }
    }

    for (size_t n = 0; n < ArrayLength(sProtoMethods); ++n) {
        jsid id = sProtoMethods[n].id;
        if (!JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, &desc))
            return false;
        if (desc.obj != proto || desc.getter || JSVAL_IS_PRIMITIVE(desc.value) ||
            n >= js::GetNumSlots(proto) || js::GetSlot(proto, n) != desc.value ||
            !JS_IsNativeFunction(JSVAL_TO_OBJECT(desc.value), sProtoMethods[n].native)) {
            *shouldCache = false;
            return true;
        }
    }

    JSObject *protoProto = js::GetObjectProto(proto);
    if (!protoProto) {
        *shouldCache = false;
        return true;
    }

    return Base::shouldCacheProtoShape(cx, protoProto, shouldCache);
}

template<class LC>
bool
ListBase<LC>::resolveNativeName(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    JS_ASSERT(xpc::WrapperFactory::IsXrayWrapper(proxy));

    for (size_t n = 0; n < ArrayLength(sProtoProperties); ++n) {
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

    for (size_t n = 0; n < ArrayLength(sProtoMethods); ++n) {
        if (id == sProtoMethods[n].id) {
            JSFunction *fun = JS_NewFunctionById(cx, sProtoMethods[n].native,
                                                 sProtoMethods[n].nargs, 0, proxy, id);
            if (!fun)
                return false;
            JSObject *funobj = JS_GetFunctionObject(fun);
            desc->value.setObject(*funobj);
            desc->attrs = JSPROP_ENUMERATE;
            desc->obj = proxy;
            desc->setter = nsnull;
            desc->getter = nsnull;
            return true;
        }
    }

    return Base::resolveNativeName(cx, proxy, id, desc);
}

template<class LC>
bool
ListBase<LC>::nativeGet(JSContext *cx, JSObject *proxy, JSObject *proto, jsid id, bool *found, Value *vp)
{
#ifdef DEBUG
    bool shouldCache;
    JS_ASSERT(shouldCacheProtoShape(cx, proto, &shouldCache) && shouldCache);
#endif

    for (size_t n = 0; n < ArrayLength(sProtoProperties); ++n) {
        if (id == sProtoProperties[n].id) {
            *found = true;
            if (!vp)
                return true;

            return sProtoProperties[n].getter(cx, proxy, id, vp);
        }
    }
    for (size_t n = 0; n < ArrayLength(sProtoMethods); ++n) {
        if (id == sProtoMethods[n].id) {
            *found = true;
            if (!vp)
                return true;

            *vp = js::GetSlot(proto, n);
            JS_ASSERT(JS_IsNativeFunction(&vp->toObject(), sProtoMethods[n].native));
            return true;
        }
    }

    JSObject *protoProto = js::GetObjectProto(proto);
    if (!protoProto) {
        *found = false;
        return true;
    }

    return Base::nativeGet(cx, proxy, protoProto, id, found, vp);
}

template<class LC>
bool
ListBase<LC>::getPropertyOnPrototype(JSContext *cx, JSObject *proxy, jsid id, bool *found,
                                     js::Value *vp)
{
    JSObject *proto = js::GetObjectProto(proxy);
    if (!proto)
        return true;

    bool hit;
    if (getProtoShape(proxy) != js::GetObjectShape(proto)) {
        if (!shouldCacheProtoShape(cx, proto, &hit))
            return false;
        if (hit)
            setProtoShape(proxy, js::GetObjectShape(proto));
    } else {
        hit = true;
    }

    if (hit) {
        if (id == s_length_id) {
            if (vp) {
                PRUint32 length;
                getListObject(proxy)->GetLength(&length);
                JS_ASSERT(int32(length) >= 0);
                vp->setInt32(length);
            }
            *found = true;
            return true;
        }
        if (!nativeGet(cx, proxy, proto, id, found, vp))
            return false;
        if (*found)
            return true;
    }

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

    bool found;
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
        int32 index = GetArrayIndexFromId(cx, id);
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

    bool found;
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
                                  uint32 index, Value *vp, bool *present)
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
ListBase<LC>::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
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
ListBase<LC>::finalize(JSContext *cx, JSObject *proxy)
{
    ListType *list = getListObject(proxy);
    nsWrapperCache *cache;
    CallQueryInterface(list, &cache);
    if (cache) {
        cache->ClearWrapper();
    }
    NS_RELEASE(list);
}


JSObject*
NoBase::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope)
{
    // We need to pass the object prototype to JS_NewObject. If we pass NULL then the JS engine
    // will look up a prototype on the global by using the class' name and we'll recurse into
    // getPrototype.
    JSObject* proto;
    if (!js_GetClassPrototype(cx, scope->GetGlobalJSObject(), JSProto_Object, &proto))
        return NULL;
    return proto;
}


}
}
}
#include "dombindings_gen.cpp"
