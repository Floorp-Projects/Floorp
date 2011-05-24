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

#include "nsIDOMNode.h"

#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"

extern XPCNativeInterface* interfaces[];

using namespace js;

namespace xpc {
namespace dom {

int NodeListBase::NodeListFamily;

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

template<>
Class NodeList<nsINodeList>::sProtoClass = {
    "NodeList",
    0,
    JS_PropertyStub,        /* addProperty */
    JS_PropertyStub,        /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template<>
JSBool
NodeList<nsINodeList>::namedItem(JSContext *cx, uintN argc, jsval *vp);

template<>
NodeList<nsINodeList>::Methods NodeList<nsINodeList>::sProtoMethods[] = {
    { nsDOMClassInfo::sItem_id, &item, 1 }
};

template<>
Class NodeList<nsIHTMLCollection>::sProtoClass = {
    "HTMLCollection",
    0,
    JS_PropertyStub,        /* addProperty */
    JS_PropertyStub,        /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template<>
JSBool
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, uintN argc, jsval *vp);

template<>
NodeList<nsIHTMLCollection>::Methods NodeList<nsIHTMLCollection>::sProtoMethods[] = {
    { nsDOMClassInfo::sItem_id, &item, 1 },
    { nsDOMClassInfo::sNamedItem_id, &namedItem, 1 }
};

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
    return js::GetProxyExtra(obj, 0).toPrivateUint32();
}

template<class T>
void
NodeList<T>::setProtoShape(JSObject *obj, uint32 shape)
{
    JS_ASSERT(js::IsProxy(obj) && js::GetProxyHandler(obj) == &NodeList<T>::instance);
    js::SetProxyExtra(obj, 0, PrivateUint32Value(shape));
}

template<class T>
bool
NodeList<T>::instanceIsNodeListObject(JSContext *cx, JSObject *obj)
{
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
    if (!instanceIsNodeListObject(cx, obj))
        return false;
    PRUint32 length;
    getNodeList(obj)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    vp->setInt32(length);
    return true;
}

template<class T>
JSBool
NodeList<T>::item(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !instanceIsNodeListObject(cx, obj))
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
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !instanceIsNodeListObject(cx, obj))
        return false;
    if (argc < 1)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    jsval *argv = JS_ARGV(cx, vp);
    xpc_qsDOMString name(cx, argv[0], &argv[0],
                         xpc_qsDOMString::eDefaultNullBehavior,
                         xpc_qsDOMString::eDefaultUndefinedBehavior);
    if (!name.IsValid())
        return JS_FALSE;
    nsIHTMLCollection *htmlCollection = getNodeList(obj);
    nsWrapperCache *cache;
    nsISupports *result = htmlCollection->GetNamedItem(name, &cache);
    return WrapObject(cx, obj, result, cache, vp);
}

template<class T>
JSObject *
NodeList<T>::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope)
{
    nsDataHashtable<nsIDHashKey, JSObject*> &cache =
        scope->GetCachedDOMPrototypes();

    JSObject *proto;
    if (cache.IsInitialized()) {
        if (cache.Get(NS_GET_TEMPLATE_IID(T), &proto))
            return proto;
    }
    else if (!cache.Init()) {
        return NULL;
    }

    proto = JS_NewObject(cx, Jsvalify(&sProtoClass), NULL, NULL);
    if (!proto)
        return NULL;
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, proto))
        return NULL;

    if (!JS_DefinePropertyById(cx, proto, nsDOMClassInfo::sLength_id, JSVAL_VOID,
                               length_getter, NULL,
                               JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED))
        return NULL;

    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoMethods); ++n) {
        jsid id = sProtoMethods[n].id;
        JSFunction *fun = JS_NewFunctionById(cx, sProtoMethods[n].native, sProtoMethods[n].nargs,
                                             0, js::GetObjectParent(proto), id);
        if (!fun)
            return NULL;
        JSObject *funobj = JS_GetFunctionObject(fun);
        if (!JS_DefinePropertyById(cx, proto, id, OBJECT_TO_JSVAL(funobj),
                                   NULL, NULL, JSPROP_ENUMERATE))
            return NULL;
    }

    if (!cache.Put(NS_GET_TEMPLATE_IID(T), proto))
        return NULL;

    return proto;
}

template<class T>
JSObject *
NodeList<T>::create(JSContext *cx, XPCWrappedNativeScope *scope, T *aNodeList,
                    nsWrapperCache* aWrapperCache)
{
    JSObject *proto = getPrototype(cx, scope);
    if (!proto)
        return NULL;
    JSObject *obj = NewProxyObject(cx, &NodeList<T>::instance,
                                   PrivateValue(aNodeList),
                                   proto, NULL);
    if (!obj)
        return NULL;

    NS_ADDREF(aNodeList);
    setProtoShape(obj, -1);

    aWrapperCache->SetWrapper(obj);

    return obj;
}

template<class T>
bool
NodeList<T>::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                      PropertyDescriptor *desc)
{
    // FIXME: expandos
    int32 index;
    if (JSID_IS_INT(id) && ((index = JSID_TO_INT(id)) >= 0)) {
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
    return JS_GetPropertyDescriptorById(cx, js::GetObjectProto(proxy), id, JSRESOLVE_QUALIFIED,
                                        desc);
}

template<class T>
bool
NodeList<T>::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                            PropertyDescriptor *desc)
{
    // FIXME: expandos
    return true;
}

template<class T>
bool
NodeList<T>::getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    // FIXME: expandos
    PRUint32 length;
    getNodeList(proxy)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    for (int32 i = 0; i < int32(length); ++i) {
        if (!props.append(INT_TO_JSID(i)))
            return false;
    }
    return true;
}

template<class T>
bool
NodeList<T>::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    // FIXME: expandos
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
    // FIXME: expandos
    int32 index;
    if (JSID_IS_INT(id) && ((index = JSID_TO_INT(id)) >= 0)) {
        if (getNodeList(proxy)->GetNodeAt(PRUint32(index))) {
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
    if (!hasOwn(cx, proxy, id, bp))
        return false;
    if (*bp)
        return true;
    JSBool found;
    if (!JS_HasPropertyById(cx, js::GetObjectProto(proxy), id, &found))
        return false;
    *bp = !!found;
    return true;
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
NodeList<T>::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp)
{
    // FIXME: expandos
    int32 index;
    if (JSID_IS_INT(id) && ((index = JSID_TO_INT(id)) >= 0)) {
        T *nodeList = getNodeList(proxy);
        nsIContent *result = nodeList->GetNodeAt(PRUint32(index));
        if (result)
            return WrapObject(cx, proxy, result, result, vp);
    }

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
    // FIXME: expandos
    return true;
}

template<class T>
bool
NodeList<T>::keys(JSContext *cx, JSObject *proxy, AutoIdVector &props)
{
    // FIXME: expandos
    return getOwnPropertyNames(cx, proxy, props);
}

template<class T>
bool
NodeList<T>::iterate(JSContext *cx, JSObject *proxy, uintN flags, Value *vp)
{
    JS_ReportError(cx, "FIXME");
    return false;
}

template<class T>
bool
NodeList<T>::hasInstance(JSContext *cx, JSObject *proxy, const Value *vp, bool *bp)
{
    *bp = vp->isObject() && js::GetObjectClass(&vp->toObject()) == &sProtoClass;
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
    nsWrapperCache* cache;
    CallQueryInterface(nodeList, &cache);
    if (cache) {
        cache->ClearWrapper();
    }
    NS_RELEASE(nodeList);
}

}
}
