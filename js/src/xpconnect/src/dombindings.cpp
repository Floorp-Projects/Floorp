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

#include "dombindings.h"
#include "xpcprivate.h"
#include "xpcquickstubs.h"
#include "XPCWrapper.h"
#include "WrapperFactory.h"

#include "nsIDOMNode.h"

#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "jsiter.h"
#include "nsWrapperCacheInlines.h"

extern XPCNativeInterface* interfaces[];

using namespace js;

namespace xpc {
namespace dom {

int HandlerFamily;

JSObject *
NodeListBase::create(JSContext *cx, XPCWrappedNativeScope *scope,
                     nsINodeList *aNodeList)
{
    return NodeList<nsINodeList>::create(cx, scope, aNodeList, aNodeList);
}

JSObject *
NodeListBase::create(JSContext *cx, XPCWrappedNativeScope *scope,
                     nsIHTMLCollection *aHTMLCollection,
                     nsWrapperCache *aWrapperCache)
{
    return NodeList<nsIHTMLCollection>::create(cx, scope, aHTMLCollection,
                                               aWrapperCache);
}

template<class T>
NodeList<T>::NodeList()
{
}

template<class T>
NodeList<T> NodeList<T>::instance;

JSBool
interface_hasInstance(JSContext *cx, JSObject *obj, const js::Value *vp, JSBool *bp);

template<>
Class NodeList<nsINodeList>::sInterfaceClass = {
    "NodeList",
    0,
    JS_PropertyStub,        /* addProperty */
    JS_PropertyStub,        /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                   /* finalize */
    NULL,                   /* reserved0 */
    NULL,                   /* checkAccess */
    NULL,                   /* call */
    NULL,                   /* construct */
    NULL,                   /* xdrObject */
    interface_hasInstance
};

template<>
NodeList<nsINodeList>::Methods NodeList<nsINodeList>::sProtoMethods[] = {
    { nsDOMClassInfo::sItem_id, &item, 1 }
};

template<>
Class NodeList<nsIHTMLCollection>::sInterfaceClass = {
    "HTMLCollection",
    0,
    JS_PropertyStub,        /* addProperty */
    JS_PropertyStub,        /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                   /* finalize */
    NULL,                   /* reserved0 */
    NULL,                   /* checkAccess */
    NULL,                   /* call */
    NULL,                   /* construct */
    NULL,                   /* xdrObject */
    interface_hasInstance
};

template<>
JSBool
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, uintN argc, jsval *vp);

template<>
NodeList<nsIHTMLCollection>::Methods NodeList<nsIHTMLCollection>::sProtoMethods[] = {
    { nsDOMClassInfo::sItem_id, &item, 1 },
    { nsDOMClassInfo::sNamedItem_id, &namedItem, 1 }
};

void
Register(nsDOMClassInfoData *aData)
{
#define REGISTER_PROTO(_dom_class, T) \
    aData[eDOMClassInfo_##_dom_class##_id].mDefineDOMInterface = NodeList<T>::getPrototype

    REGISTER_PROTO(NodeList, nsINodeList);
    REGISTER_PROTO(HTMLCollection, nsIHTMLCollection);

#undef REGISTER_PROTO
}

bool
DefineConstructor(JSContext *cx, JSObject *obj, DefineInterface aDefine)
{
    return !!aDefine(cx, XPCWrappedNativeScope::FindInJSObjectScope(cx, obj));
}

template<class T>
T*
NodeList<T>::getNodeList(JSObject *obj)
{
    JS_ASSERT(objIsNodeList(obj));
    return static_cast<T *>(js::GetProxyPrivate(obj).toPrivate());
}

template<class T>
uint32
NodeList<T>::getProtoShape(JSObject *obj)
{
    JS_ASSERT(js::IsProxy(obj) && js::GetProxyHandler(obj) == &NodeList<T>::instance);
    return js::GetProxyExtra(obj, JSPROXYSLOT_PROTOSHAPE).toPrivateUint32();
}

template<class T>
void
NodeList<T>::setProtoShape(JSObject *obj, uint32 shape)
{
    JS_ASSERT(js::IsProxy(obj) && js::GetProxyHandler(obj) == &NodeList<T>::instance);
    js::SetProxyExtra(obj, JSPROXYSLOT_PROTOSHAPE, PrivateUint32Value(shape));
}

template<class T>
bool
NodeList<T>::instanceIsNodeListObject(JSContext *cx, JSObject *obj, JSObject *callee)
{
    if (XPCWrapper::IsSecurityWrapper(obj)) {
        if (callee && js::GetObjectGlobal(obj) == js::GetObjectGlobal(callee)) {
            obj = js::UnwrapObject(obj);
        }
        else {
            obj = XPCWrapper::Unwrap(cx, obj);
            if (!obj)
                return xpc_qsThrow(cx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
        }
    }

    if (!js::IsProxy(obj) || (js::GetProxyHandler(obj) != &NodeList<T>::instance)) {
        // FIXME: Throw a proper DOM exception.
        JS_ReportError(cx, "type error: wrong object");
        return false;
    }
    return true;
}

static bool
WrapObject(JSContext *cx, JSObject *scope, nsISupports *result,
           nsWrapperCache *cache, jsval *vp)
{
    if (xpc_FastGetCachedWrapper(cache, scope, vp))
        return true;
    XPCLazyCallContext lccx(JS_CALLER, cx, scope);
    qsObjectHelper helper(result, cache);
    return xpc_qsXPCOMObjectToJsval(lccx, helper, &NS_GET_IID(nsIDOMNode),
                                    &interfaces[k_nsIDOMNode], vp);
}

template<class T>
JSBool
NodeList<T>::length_getter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (!instanceIsNodeListObject(cx, obj, NULL))
        return false;
    PRUint32 length;
    getNodeList(obj)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    vp->setInt32(length);
    return true;
}

template<>
nsISupports*
NodeList<nsINodeList>::getNamedItem(nsINodeList *list, const nsAString& aName,
                                    nsWrapperCache **aCache)
{
    return NULL;
}
template<>
nsISupports*
NodeList<nsIHTMLCollection>::getNamedItem(nsIHTMLCollection *list, const nsAString& aName,
                                          nsWrapperCache **aCache)
{
    return list->GetNamedItem(aName, aCache);
}

template<class T>
JSBool
NodeList<T>::item(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    if (!obj || !instanceIsNodeListObject(cx, obj, callee))
        return false;
    if (argc < 1)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    jsval *argv = JS_ARGV(cx, vp);
    uint32 u;
    if (!JS_ValueToECMAUint32(cx, argv[0], &u))
        return false;
    T *nodeList = getNodeList(obj);
    nsIContent *result = nodeList->GetNodeAt(u);
    return WrapObject(cx, obj, result, result, vp);
}


template<>
JSBool
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, JSObject *obj, jsval *name, jsval *vp)
{
    xpc_qsDOMString nameString(cx, *name, name,
                               xpc_qsDOMString::eDefaultNullBehavior,
                               xpc_qsDOMString::eDefaultUndefinedBehavior);
    if (!nameString.IsValid())
        return JS_FALSE;
    nsIHTMLCollection *collection = getNodeList(obj);
    nsWrapperCache *cache;
    nsISupports *result = getNamedItem(collection, nameString, &cache);
    if (!result) {
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }
    return WrapObject(cx, obj, result, cache, vp);
}

template<>
JSBool
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
    if (!obj || !instanceIsNodeListObject(cx, obj, callee))
        return false;
    if (argc < 1)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    jsval *argv = JS_ARGV(cx, vp);
    return namedItem(cx, obj, &argv[0], vp);
}

JSBool
interface_hasInstance(JSContext *cx, JSObject *obj, const js::Value *vp, JSBool *bp)
{
    if (vp->isObject()) {
        jsval prototype;
        if (!JS_GetPropertyById(cx, obj, nsDOMClassInfo::sPrototype_id, &prototype) ||
            JSVAL_IS_PRIMITIVE(prototype)) {
            JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                         JSMSG_THROW_TYPE_ERROR);
            return JS_FALSE;
        }

        JSObject *other = &vp->toObject();
        if (instanceIsProxy(other)) {
            ProxyHandler *handler = static_cast<ProxyHandler*>(js::GetProxyHandler(other));
            if (handler->isInstanceOf(JSVAL_TO_OBJECT(prototype))) {
                *bp = JS_TRUE;
            }
            else {
                JSObject *protoObj = JSVAL_TO_OBJECT(prototype);
                JSObject *proto = other;
                while ((proto = JS_GetPrototype(cx, proto))) {
                    if (proto == protoObj) {
                        *bp = JS_TRUE;
                        return JS_TRUE;
                    }
                }
                *bp = JS_FALSE;
            }

            return JS_TRUE;
        }
    }

    *bp = JS_FALSE;
    return JS_TRUE;
}

template<class T>
JSObject *
NodeList<T>::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope)
{
    nsDataHashtable<nsDepCharHashKey, JSObject*> &cache =
        scope->GetCachedDOMPrototypes();

    JSObject *interfacePrototype;
    if (cache.IsInitialized()) {
        if (cache.Get(sInterfaceClass.name, &interfacePrototype))
            return interfacePrototype;
    }
    else if (!cache.Init()) {
        return NULL;
    }

    JSObject *global = scope->GetGlobalJSObject();

    // We need to pass the object prototype to JS_NewObject. If we pass NULL then the JS engine
    // will look up a prototype on the global by using the class' name and we'll recurse into
    // getPrototype.
    JSObject* proto;
    if (!js_GetClassPrototype(cx, global, JSProto_Object, &proto))
        return NULL;

    interfacePrototype = JS_NewObject(cx, NULL, proto, global);
    if (!interfacePrototype)
        return NULL;

    if (!JS_DefinePropertyById(cx, interfacePrototype, nsDOMClassInfo::sLength_id,
                               JSVAL_VOID, length_getter, NULL,
                               JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED))
        return NULL;

    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoMethods); ++n) {
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
        !JS_DefinePropertyById(cx, interface, nsDOMClassInfo::sPrototype_id,
                               OBJECT_TO_JSVAL(interfacePrototype), nsnull, nsnull,
                               JSPROP_PERMANENT | JSPROP_READONLY))
        return NULL;

    if (!JS_DefinePropertyById(cx, interfacePrototype, nsDOMClassInfo::sConstructor_id,
                               OBJECT_TO_JSVAL(interface), nsnull, nsnull, 0))
        return NULL;

    if (!JS_DefineProperty(cx, global, sInterfaceClass.name, OBJECT_TO_JSVAL(interface), NULL,
                           NULL, 0))
        return NULL;

    if (!cache.Put(sInterfaceClass.name, interfacePrototype))
        return NULL;

    return interfacePrototype;
}

template<class T>
JSObject *
NodeList<T>::create(JSContext *cx, XPCWrappedNativeScope *scope, T *aNodeList,
                    nsWrapperCache* aWrapperCache)
{
    nsINode *nativeParent = aNodeList->GetParentObject();
    if (!nativeParent)
        return NULL;

    jsval v;
    if (!WrapObject(cx, scope->GetGlobalJSObject(), nativeParent, nativeParent, &v))
        return NULL;

    JSObject *parent = JSVAL_TO_OBJECT(v);

    JSAutoEnterCompartment ac;
    if (js::GetObjectGlobal(parent) != scope->GetGlobalJSObject()) {
        if (!ac.enter(cx, parent))
            return NULL;

        scope = XPCWrappedNativeScope::FindInJSObjectScope(cx, parent);
    }

    JSObject *proto = getPrototype(cx, scope);
    if (!proto)
        return NULL;
    JSObject *obj = NewProxyObject(cx, &NodeList<T>::instance,
                                   PrivateValue(aNodeList),
                                   proto, parent);
    if (!obj)
        return NULL;

    NS_ADDREF(aNodeList);
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

template<class T>
bool
NodeList<T>::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                      PropertyDescriptor *desc)
{
    JSObject *expando = getExpandoObject(proxy);
    uintN flags = (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED;
    if (expando && !JS_GetPropertyDescriptorById(cx, expando, id, flags, desc))
        return false;
    if (desc->obj) {
        // Pretend the property lives on the wrapper.
        desc->obj = proxy;
        return true;
    }

    bool isNumber;
    int32 index = nsDOMClassInfo::GetArrayIndexFromId(cx, id, &isNumber);
    if (isNumber && index >= 0) {
        T *nodeList = getNodeList(proxy);
        nsIContent *result = nodeList->GetNodeAt(PRUint32(index));
        if (result) {
            jsval v;
            if (!WrapObject(cx, proxy, result, result, &v))
                return false;
            desc->obj = proxy;
            desc->value = v;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE;
            desc->getter = NULL;
            desc->setter = NULL;
            desc->shortid = 0;
            return true;
        }
    }

    bool ok;
    Value v;
    if (namedItem(cx, proxy, id, &v, &ok)) {
        desc->obj = proxy;
        desc->value = v;
        desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->shortid = 0;
        return ok;
    }

    desc->obj = NULL;
    return true;
}

template<class T>
bool
NodeList<T>::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                   PropertyDescriptor *desc)
{
    if (!getOwnPropertyDescriptor(cx, proxy, id, set, desc))
        return false;
    if (desc->obj)
        return true;
    if (WrapperFactory::IsXrayWrapper(proxy))
        return resolveNativeName(cx, proxy, id, desc);
    return JS_GetPropertyDescriptorById(cx, js::GetObjectProto(proxy), id, JSRESOLVE_QUALIFIED,
                                        desc);
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

template<class T>
JSObject *
NodeList<T>::ensureExpandoObject(JSContext *cx, JSObject *obj)
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
        expando->setPrivate(js::GetProxyPrivate(obj).toPrivate());
    }
    return expando;
}

template<class T>
bool
NodeList<T>::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                            PropertyDescriptor *desc)
{
    JSObject *expando = ensureExpandoObject(cx, proxy);
    if (!expando)
        return false;

    return JS_DefinePropertyById(cx, expando, id, desc->value, desc->getter, desc->setter,
                                 desc->attrs);
}

template<class T>
bool
NodeList<T>::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    JSObject *expando = getExpandoObject(proxy);
    if (expando && !GetPropertyNames(cx, expando, JSITER_OWNONLY | JSITER_HIDDEN, &props))
        return false;

    PRUint32 length;
    getNodeList(proxy)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    for (int32 i = 0; i < int32(length); ++i) {
        if (!props.append(INT_TO_JSID(i)))
            return false;
    }
    // FIXME: Add named items?
    return true;
}

template<class T>
bool
NodeList<T>::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
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

template<class T>
bool
NodeList<T>::enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    // FIXME: enumerate proto as well
    return getOwnPropertyNames(cx, proxy, props);
}

template<class T>
bool
NodeList<T>::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    vp->setUndefined();
    return true;
}

template<class T>
bool
NodeList<T>::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    JSObject *expando = getExpandoObject(proxy);
    if (expando) {
        JSBool b = JS_TRUE;
        JSBool ok = JS_HasPropertyById(cx, expando, id, &b);
        *bp = !!b;
        if (!ok || *bp)
            return ok;
    }

    bool isNumber;
    int32 index = nsDOMClassInfo::GetArrayIndexFromId(cx, id, &isNumber);
    if (isNumber && index >= 0) {
        if (getNodeList(proxy)->GetNodeAt(PRUint32(index))) {
            *bp = true;
            return true;
        }
    }

    if (JSID_IS_STRING(id)) {
        jsval v = STRING_TO_JSVAL(JSID_TO_STRING(id));
        xpc_qsDOMString nameString(cx, v, &v,
                                   xpc_qsDOMString::eDefaultNullBehavior,
                                   xpc_qsDOMString::eDefaultUndefinedBehavior);
        nsWrapperCache *cache;
        if (getNamedItem(getNodeList(proxy), nameString, &cache)) {
            *bp = true;
            return true;
        }
    }

    *bp = false;
    return true;
}

template<class T>
bool
NodeList<T>::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    return ProxyHandler::has(cx, proxy, id, bp);
}

template<class T>
bool
NodeList<T>::cacheProtoShape(JSContext *cx, JSObject *proxy, JSObject *proto)
{
    JSPropertyDescriptor desc;
    if (!JS_GetPropertyDescriptorById(cx, proto, nsDOMClassInfo::sLength_id, JSRESOLVE_QUALIFIED, &desc))
        return false;
    if (desc.obj != proto || desc.getter != length_getter)
        return true; // don't cache

    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoMethods); ++n) {
        jsid id = sProtoMethods[n].id;
        if (!JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, &desc))
            return false;
        if (desc.obj != proto || desc.getter || JSVAL_IS_PRIMITIVE(desc.value) ||
            n >= js::GetNumSlots(proto) || js::GetSlot(proto, n) != desc.value ||
            !JS_IsNativeFunction(JSVAL_TO_OBJECT(desc.value), sProtoMethods[n].native)) {
            return true; // don't cache
        }
    }

    setProtoShape(proxy, js::GetObjectShape(proto));
    return true;
}

template<class T>
bool
NodeList<T>::checkForCacheHit(JSContext *cx, JSObject *proxy, JSObject *receiver, JSObject *proto,
                              jsid id, Value *vp, bool *hitp)
{
    if (getProtoShape(proxy) != js::GetObjectShape(proto)) {
        if (!cacheProtoShape(cx, proxy, proto))
            return false;
        if (getProtoShape(proxy) != js::GetObjectShape(proto)) {
            *hitp = false;
            return JS_GetPropertyById(cx, proto, id, vp);
        }
    }
    *hitp = true;
    return true;
}

template<class T>
bool
NodeList<T>::resolveNativeName(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc)
{
    JS_ASSERT(WrapperFactory::IsXrayWrapper(proxy));

    if (id == nsDOMClassInfo::sLength_id) {
        desc->attrs = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED;
        desc->obj = proxy;
        desc->setter = nsnull;
        desc->getter = length_getter;
        return true;
    }

    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoMethods); ++n) {
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

    return true;
}

template<>
inline bool
NodeList<nsINodeList>::namedItem(JSContext *cx, JSObject *proxy, jsid id, Value *vp, bool *result)
{
    return false;
}
template<>
inline bool
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, JSObject *proxy, jsid id, Value *vp, bool *result)
{
    if (!JSID_IS_STRING(id))
        return false;

    jsval v = STRING_TO_JSVAL(JSID_TO_STRING(id));
    *result = namedItem(cx, proxy, &v, vp);

    // We treat failure as having handled the get.
    return !*result || !vp->isNull();
}

template<class T>
bool
NodeList<T>::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    JSObject *expando = getExpandoObject(proxy);
    if (expando) {
        JSBool hasProp;
        if (!JS_HasPropertyById(cx, expando, id, &hasProp))
            return false;
        if (hasProp)
            return JS_GetPropertyById(cx, expando, id, vp);
    }

    bool isNumber;
    int32 index = nsDOMClassInfo::GetArrayIndexFromId(cx, id, &isNumber);
    if (isNumber && index >= 0) {
        T *nodeList = getNodeList(proxy);
        nsIContent *result = nodeList->GetNodeAt(PRUint32(index));
        if (result)
            return WrapObject(cx, proxy, result, result, vp);
    }

    bool ok;
    if (namedItem(cx, proxy, id, vp, &ok))
        return ok;

    JSObject *proto = js::GetObjectProto(proxy);
    bool hit;
    if (!checkForCacheHit(cx, proxy, receiver, proto, id, vp, &hit))
        return false;
    if (hit) {
        if (id == nsDOMClassInfo::sLength_id) {
            PRUint32 length;
            getNodeList(proxy)->GetLength(&length);
            JS_ASSERT(int32(length) >= 0);
            vp->setInt32(length);
            return true;
        }
        for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoMethods); ++n) {
            if (id == sProtoMethods[n].id) {
                *vp = js::GetSlot(proto, n);
                JS_ASSERT(JS_IsNativeFunction(&vp->toObject(), sProtoMethods[n].native));
                return true;
            }
        }
    }

    return JS_GetPropertyById(cx, proto, id, vp);
}

template<class T>
bool
NodeList<T>::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                 Value *vp)
{
    return ProxyHandler::set(cx, proxy, proxy, id, strict, vp);
}

template<class T>
bool
NodeList<T>::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    return ProxyHandler::keys(cx, proxy, props);
}

template<class T>
bool
NodeList<T>::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    return ProxyHandler::iterate(cx, proxy, flags, vp);
}

template<class T>
bool
NodeList<T>::hasInstance(JSContext *cx, JSObject *proxy, const Value *vp, bool *bp)
{
    *bp = vp->isObject() && js::GetObjectClass(&vp->toObject()) == &sInterfaceClass;
    return true;
}

template<>
JSString *
NodeList<nsIHTMLCollection>::obj_toString(JSContext *cx, JSObject *proxy)
{
    return JS_NewStringCopyZ(cx, "[object HTMLCollection]");
}

template<>
JSString *
NodeList<nsINodeList>::obj_toString(JSContext *cx, JSObject *proxy)
{
    return JS_NewStringCopyZ(cx, "[object NodeList]");
}

template<class T>
void
NodeList<T>::finalize(JSContext *cx, JSObject *proxy)
{
    T *nodeList = getNodeList(proxy);
    nsWrapperCache *cache;
    CallQueryInterface(nodeList, &cache);
    if (cache) {
        cache->ClearWrapper();
    }
    NS_RELEASE(nodeList);
}

}
}
