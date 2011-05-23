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

extern XPCNativeInterface* interfaces[];

using namespace js;

namespace xpc {
namespace dom {

static int NodeListFamily;

NodeList::NodeList() : ProxyHandler(&NodeListFamily)
{
}

NodeList NodeList::instance;

static Class NodeListProtoClass = {
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

nsINodeList *
NodeList::getNodeList(JSObject *obj)
{
    JS_ASSERT(objIsNodeList(obj));
    return static_cast<nsINodeList *>(js::GetProxyPrivate(obj).toPrivate());
}

uint32
NodeList::getProtoShape(JSObject *obj)
{
    JS_ASSERT(js::IsProxy(obj) && js::GetProxyHandler(obj) == &NodeList::instance);
    return js::GetProxyExtra(obj, 0).toPrivateUint32();
}

void
NodeList::setProtoShape(JSObject *obj, uint32 shape)
{
    JS_ASSERT(js::IsProxy(obj) && js::GetProxyHandler(obj) == &NodeList::instance);
    js::SetProxyExtra(obj, 0, PrivateUint32Value(shape));
}

bool
NodeList::InstanceIsNodeListObject(JSContext *cx, JSObject *obj)
{
    if (!js::IsProxy(obj) || (js::GetProxyHandler(obj) != &NodeList::instance)) {
        // FIXME: Throw a proper DOM exception.
        JS_ReportError(cx, "type error: wrong object");
        return false;
    }
    return true;
}

static bool
WrapObject(JSContext *cx, JSObject *scope, nsIContent *result, jsval *vp)
{
    nsWrapperCache *cache = result;
    if (xpc_FastGetCachedWrapper(cache, scope, vp))
        return true;
    XPCLazyCallContext lccx(JS_CALLER, cx, scope);
    qsObjectHelper helper(result, cache);
    return xpc_qsXPCOMObjectToJsval(lccx, helper, &NS_GET_IID(nsIDOMNode),
                                    &interfaces[k_nsIDOMNode], vp);
}

JSBool
NodeList::length_getter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (!InstanceIsNodeListObject(cx, obj))
        return false;
    PRUint32 length;
    getNodeList(obj)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    vp->setInt32(length);
    return true;
}

JSBool
NodeList::item(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !InstanceIsNodeListObject(cx, obj))
        return false;
    if (argc < 1)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    jsval *argv = JS_ARGV(cx, vp);
    uint32 u;
    if (!JS_ValueToECMAUint32(cx, argv[0], &u))
        return false;
    nsINodeList *nodeList = getNodeList(obj);
    nsIContent *result = nodeList->GetNodeAt(u);
    return WrapObject(cx, obj, result, vp);
}

JSObject *
NodeList::getPrototype(JSContext *cx)
{
    // FIXME: This should be cached, not recreated every time.
    JSObject *proto = JS_NewObject(cx, Jsvalify(&NodeListProtoClass), NULL, NULL);
    if (!proto)
        return NULL;
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, proto))
        return NULL;
    if (!JS_DefinePropertyById(cx, proto, nsDOMClassInfo::sLength_id, JSVAL_VOID,
                               length_getter, NULL,
                               JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED))
        return NULL;
    JSFunction *fun = JS_NewFunctionById(cx, item, 1, 0, js::GetObjectParent(proto),
                                         nsDOMClassInfo::sItem_id);
    if (!fun)
        return NULL;
    JSObject *funobj = JS_GetFunctionObject(fun);
    if (!JS_DefinePropertyById(cx, proto, nsDOMClassInfo::sItem_id, OBJECT_TO_JSVAL(funobj),
                               NULL, NULL, JSPROP_ENUMERATE))
        return NULL;
    return proto;
}

JSObject *
NodeList::create(JSContext *cx, nsINodeList *aNodeList)
{
    JSObject *proto = getPrototype(cx);
    if (!proto)
        return NULL;
    JSObject *obj = js::NewProxyObject(cx, &NodeList::instance,
                                       PrivateValue(aNodeList),
                                       proto, NULL);
    if (!obj)
        return NULL;

    NS_ADDREF(aNodeList);
    setProtoShape(obj, -1);

    return obj;
}

bool
NodeList::getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                   PropertyDescriptor *desc)
{
    // FIXME: expandos
    int32 index;
    if (JSID_IS_INT(id) && ((index = JSID_TO_INT(id)) >= 0)) {
        nsINodeList *nodeList = getNodeList(proxy);
        nsIContent *result = nodeList->GetNodeAt(PRUint32(index));
        if (result) {
            jsval v;
            if (!WrapObject(cx, proxy, result, &v))
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

bool
NodeList::getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                PropertyDescriptor *desc)
{
    if (!getOwnPropertyDescriptor(cx, proxy, id, set, desc))
        return false;
    if (desc->obj)
        return true;
    return JS_GetPropertyDescriptorById(cx, js::GetObjectProto(proxy), id, JSRESOLVE_QUALIFIED,
                                        desc);
}

bool
NodeList::defineProperty(JSContext *cx, JSObject *proxy, jsid id,
                         PropertyDescriptor *desc)
{
    // FIXME: expandos
    return true;
}

bool
NodeList::getOwnPropertyNames(JSContext *cx, JSObject *proxy, js::AutoIdVector &props)
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

bool
NodeList::delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
{
    // FIXME: expandos
    return true;
}

bool
NodeList::enumerate(JSContext *cx, JSObject *proxy, js::AutoIdVector &props)
{
    // FIXME: enumerate proto as well
    return getOwnPropertyNames(cx, proxy, props);
}

bool
NodeList::fix(JSContext *cx, JSObject *proxy, Value *vp)
{
    vp->setUndefined();
    return true;
}

bool
NodeList::hasOwn(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
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

bool
NodeList::has(JSContext *cx, JSObject *proxy, jsid id, bool *bp)
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

bool
NodeList::get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, js::Value *vp)
{
    // FIXME: expandos
    int32 index;
    if (JSID_IS_INT(id) && ((index = JSID_TO_INT(id)) >= 0)) {
        nsINodeList *nodeList = getNodeList(proxy);
        nsIContent *result = nodeList->GetNodeAt(PRUint32(index));
        if (result)
            return WrapObject(cx, proxy, result, vp);
    }

    JSObject *proto = js::GetObjectProto(proxy);

    if (id == nsDOMClassInfo::sLength_id) {
        uint32 kshape = getProtoShape(proxy);
        uint32 pshape = js::GetObjectShape(proto);
        do {
            if (kshape != pshape) {
                JSPropertyDescriptor desc;
                if (!JS_GetPropertyDescriptorById(cx, proto, id,
                                                  JSRESOLVE_QUALIFIED,
                                                  &desc)) {
                    return false;
                }
                if (desc.obj == proto &&
                    desc.getter == length_getter) {
                    setProtoShape(proxy, pshape);
                } else {
                    break;
                }
            }

            PRUint32 length;
            getNodeList(proxy)->GetLength(&length);
            JS_ASSERT(int32(length) >= 0);
            vp->setInt32(length);
            return true;
        } while (0);
    }

    return JS_GetPropertyById(cx, proto, id, vp);
}

bool
NodeList::set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
              js::Value *vp)
{
    // FIXME: expandos
    return true;
}

bool
NodeList::keys(JSContext *cx, JSObject *proxy, js::AutoIdVector &props)
{
    // FIXME: expandos
    return getOwnPropertyNames(cx, proxy, props);
}

bool
NodeList::iterate(JSContext *cx, JSObject *proxy, uintN flags, js::Value *vp)
{
    JS_ReportError(cx, "FIXME");
    return false;
}

bool
NodeList::hasInstance(JSContext *cx, JSObject *proxy, const js::Value *vp, bool *bp)
{
    *bp = vp->isObject() && js::GetObjectClass(&vp->toObject()) == &NodeListProtoClass;
    return true;
}

JSString *
NodeList::obj_toString(JSContext *cx, JSObject *proxy)
{
    return JS_NewStringCopyZ(cx, "[object NodeList]");
}

void
NodeList::finalize(JSContext *cx, JSObject *proxy)
{
    nsINodeList *nodeList = getNodeList(proxy);
    NS_RELEASE(nodeList);
}

}
}
