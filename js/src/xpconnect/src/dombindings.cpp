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
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "jsiter.h"
#include "nsWrapperCacheInlines.h"

using namespace js;

namespace xpc {
namespace dom {


static jsid s_constructor_id = JSID_VOID;
static jsid s_prototype_id = JSID_VOID;

static jsid s_length_id = JSID_VOID;
static jsid s_item_id = JSID_VOID;
static jsid s_namedItem_id = JSID_VOID;

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
           SET_JSID_TO_STRING(cx, item) &&
           SET_JSID_TO_STRING(cx, namedItem);
}


int HandlerFamily;


JSBool
Throw(JSContext *cx, nsresult rv)
{
    XPCThrower::Throw(rv, cx);
    return JS_FALSE;
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

JSObject *
NodeListBase::create(JSContext *cx, XPCWrappedNativeScope *scope,
                     nsINodeList *aNodeList, bool *triedToWrap)
{
    return NodeList<nsINodeList>::create(cx, scope, aNodeList, aNodeList, triedToWrap);
}

JSObject *
NodeListBase::create(JSContext *cx, XPCWrappedNativeScope *scope,
                     nsIHTMLCollection *aHTMLCollection,
                     nsWrapperCache *aWrapperCache, bool *triedToWrap)
{
    return NodeList<nsIHTMLCollection>::create(cx, scope, aHTMLCollection,
                                               aWrapperCache, triedToWrap);
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
NodeList<nsINodeList>::Properties NodeList<nsINodeList>::sProtoProperties[] = {
    { s_length_id, length_getter, NULL }
};

template<>
NodeList<nsINodeList>::Methods NodeList<nsINodeList>::sProtoMethods[] = {
    { s_item_id, &item, 1 }
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
NodeList<nsIHTMLCollection>::Properties NodeList<nsIHTMLCollection>::sProtoProperties[] = {
    { s_length_id, length_getter, NULL }
};

template<>
NodeList<nsIHTMLCollection>::Methods NodeList<nsIHTMLCollection>::sProtoMethods[] = {
    { s_item_id, &item, 1 },
    { s_namedItem_id, &namedItem, 1 }
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
DefineConstructor(JSContext *cx, JSObject *obj, DefineInterface aDefine, nsresult *aResult)
{
    bool enabled;
    bool defined = !!aDefine(cx, XPCWrappedNativeScope::FindInJSObjectScope(cx, obj), &enabled);
    NS_ASSERTION(!defined || enabled,
                 "We defined a constructor but the new bindings are disabled?");
    *aResult = defined ? NS_OK : NS_ERROR_FAILURE;
    return enabled;
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
                return Throw(cx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
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
    qsObjectHelper helper(result, cache);
    return XPCOMObjectToJsval(cx, scope, helper, true, vp);
}

template<class T>
JSBool
NodeList<T>::length_getter(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!instanceIsNodeListObject(cx, obj, NULL))
        return false;
    PRUint32 length;
    getNodeList(obj)->GetLength(&length);
    JS_ASSERT(int32(length) >= 0);
    *vp = UINT_TO_JSVAL(length);
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
        return Throw(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    jsval *argv = JS_ARGV(cx, vp);
    uint32 u;
    if (!JS_ValueToECMAUint32(cx, argv[0], &u))
        return false;
    T *nodeList = getNodeList(obj);
    nsIContent *result = nodeList->GetNodeAt(u);
    return WrapObject(cx, obj, result, result, vp);
}


template<>
bool
NodeList<nsINodeList>::hasNamedItem(jsid id)
{
    return false;
}

template<>
bool
NodeList<nsINodeList>::namedItem(JSContext *cx, JSObject *obj, jsval *name,
                                 nsISupports **result, nsWrapperCache **cache,
                                 bool *hasResult)
{
    *hasResult = false;
    return true;
}

template<>
bool
NodeList<nsIHTMLCollection>::hasNamedItem(jsid id)
{
    return JSID_IS_STRING(id);
}

template<>
bool
NodeList<nsIHTMLCollection>::namedItem(JSContext *cx, JSObject *obj, jsval *name,
                                       nsISupports **result, nsWrapperCache **cache,
                                       bool *hasResult)
{
    xpc_qsDOMString nameString(cx, *name, name,
                               xpc_qsDOMString::eDefaultNullBehavior,
                               xpc_qsDOMString::eDefaultUndefinedBehavior);
    if (!nameString.IsValid())
        return false;
    nsIHTMLCollection *collection = getNodeList(obj);
    *result = getNamedItem(collection, nameString, cache);
    *hasResult = !!*result;
    return true;
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
        return Throw(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
    jsval *argv = JS_ARGV(cx, vp);
    bool hasResult;
    nsISupports *result;
    nsWrapperCache *cache;
    if (!namedItem(cx, obj, &argv[0], &result, &cache, &hasResult))
        return JS_FALSE;
    if (!hasResult) {
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }
    return WrapObject(cx, obj, result, cache, vp);
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
NodeList<T>::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope, bool *enabled)
{
    if(!scope->NewDOMBindingsEnabled()) {
        *enabled = false;
        return NULL;
    }

    *enabled = true;

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

    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoProperties); ++n) {
        jsid id = sProtoProperties[n].id;
        uintN attrs = JSPROP_ENUMERATE | JSPROP_SHARED;
        if (!sProtoProperties[n].setter)
            attrs |= JSPROP_READONLY;
        if (!JS_DefinePropertyById(cx, interfacePrototype, id, JSVAL_VOID,
                                   sProtoProperties[n].getter, sProtoProperties[n].setter, attrs))
            return NULL;
    }

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

template<class T>
JSObject *
NodeList<T>::create(JSContext *cx, XPCWrappedNativeScope *scope, T *aNodeList,
                    nsWrapperCache* aWrapperCache, bool *triedToWrap)
{
    *triedToWrap = true;

    JSObject *parent = WrapNativeParent(cx, scope->GetGlobalJSObject(), aNodeList->GetParentObject());
    if (!parent)
        return NULL;

    JSAutoEnterCompartment ac;
    if (js::GetObjectGlobal(parent) != scope->GetGlobalJSObject()) {
        if (!ac.enter(cx, parent))
            return NULL;

        scope = XPCWrappedNativeScope::FindInJSObjectScope(cx, parent);
    }

    JSObject *proto = getPrototype(cx, scope, triedToWrap);
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
    }
    return IdToInt32(cx, id);
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

    if (hasNamedItem(id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        bool hasResult;
        nsISupports *result;
        nsWrapperCache *cache;
        if (!namedItem(cx, proxy, &name, &result, &cache, &hasResult))
            return false;
        if (hasResult) {
            jsval v;
            if (!WrapObject(cx, proxy, result, cache, &v))
                return false;
            desc->obj = proxy;
            desc->value = v;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE;
            desc->getter = NULL;
            desc->setter = NULL;
            desc->shortid = 0;
        }
        else {
            desc->obj = NULL;
        }
        return true;
    }

    int32 index = GetArrayIndexFromId(cx, id);
    if (index >= 0) {
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

    if (hasNamedItem(id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        nsISupports *result;
        nsWrapperCache *cache;
        return namedItem(cx, proxy, &name, &result, &cache, bp);
    }

    int32 index = GetArrayIndexFromId(cx, id);
    if (index >= 0) {
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
    return ProxyHandler::has(cx, proxy, id, bp);
}

template<class T>
bool
NodeList<T>::cacheProtoShape(JSContext *cx, JSObject *proxy, JSObject *proto)
{
    JSPropertyDescriptor desc;
    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoProperties); ++n) {
        jsid id = sProtoProperties[n].id;
        if (!JS_GetPropertyDescriptorById(cx, proto, id, JSRESOLVE_QUALIFIED, &desc))
            return false;
        if (desc.obj != proto || desc.getter != sProtoProperties[n].getter ||
            desc.setter != sProtoProperties[n].setter)
            return true; // don't cache
    }

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

    for (size_t n = 0; n < NS_ARRAY_LENGTH(sProtoProperties); ++n) {
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

    if (hasNamedItem(id)) {
        jsval name = STRING_TO_JSVAL(JSID_TO_STRING(id));
        bool hasResult;
        nsISupports *result;
        nsWrapperCache *cache;
        if (!namedItem(cx, proxy, &name, &result, &cache, &hasResult))
            return false;
        if (hasResult)
            return WrapObject(cx, proxy, result, cache, vp);
    }

    int32 index = GetArrayIndexFromId(cx, id);
    if (index >= 0) {
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
        if (id == s_length_id) {
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

template<class T>
JSString *
NodeList<T>::obj_toString(JSContext *cx, JSObject *proxy)
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

template
nsIHTMLCollection*
NodeList<nsIHTMLCollection>::getNodeList(JSObject *obj);

}
}
