/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptParent.h"
#include "mozilla/dom/ContentParent.h"
#include "nsJSUtils.h"
#include "jsfriendapi.h"
#include "jsproxy.h"
#include "jswrapper.h"
#include "HeapAPI.h"
#include "xpcprivate.h"
#include "mozilla/Casting.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;
using namespace mozilla::dom;

JavaScriptParent::JavaScriptParent()
  : refcount_(1),
    inactive_(false)
{
}

static inline JavaScriptParent *
ParentOf(JSObject *obj)
{
    MOZ_ASSERT(JavaScriptParent::IsCPOW(obj));
    return reinterpret_cast<JavaScriptParent *>(GetProxyExtra(obj, 0).toPrivate());
}

ObjectId
JavaScriptParent::idOf(JSObject *obj)
{
    MOZ_ASSERT(JavaScriptParent::IsCPOW(obj));

    Value v = GetProxyExtra(obj, 1);
    MOZ_ASSERT(v.isDouble());

    ObjectId objId = BitwiseCast<uint64_t>(v.toDouble());
    MOZ_ASSERT(findObject(objId) == obj);
    MOZ_ASSERT(objId);

    return objId;
}

int sCPOWProxyHandler;

class CPOWProxyHandler : public BaseProxyHandler
{
  public:
    CPOWProxyHandler()
      : BaseProxyHandler(&sCPOWProxyHandler) {}
    virtual ~CPOWProxyHandler() {}

    virtual bool finalizeInBackground(Value priv) MOZ_OVERRIDE {
        return false;
    }

    virtual bool preventExtensions(JSContext *cx, HandleObject proxy) MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc,
                                       unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy,
                                          HandleId id, MutableHandle<JSPropertyDescriptor> desc,
                                          unsigned flags) MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyNames(JSContext *cx, HandleObject proxy,
                                     AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE;

    virtual bool has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                     HandleId id, MutableHandleValue vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
                     JS::HandleId id, bool strict, JS::MutableHandleValue vp) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, HandleObject proxy, AutoIdVector &props) MOZ_OVERRIDE;

    virtual bool isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) MOZ_OVERRIDE;
    virtual bool call(JSContext *cx, HandleObject proxy, const CallArgs &args) MOZ_OVERRIDE;
    virtual bool objectClassIs(HandleObject obj, js::ESClassValue classValue, JSContext *cx) MOZ_OVERRIDE;
    virtual const char* className(JSContext *cx, HandleObject proxy) MOZ_OVERRIDE;
    virtual void finalize(JSFreeOp *fop, JSObject *proxy) MOZ_OVERRIDE;

    static CPOWProxyHandler singleton;
};

CPOWProxyHandler CPOWProxyHandler::singleton;

bool
CPOWProxyHandler::preventExtensions(JSContext *cx, HandleObject proxy)
{
    return ParentOf(proxy)->preventExtensions(cx, proxy);
}

bool
JavaScriptParent::preventExtensions(JSContext *cx, HandleObject proxy)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    if (!CallPreventExtensions(objId, &status))
        return ipcfail(cx);

    return ok(cx, status);
}

bool
CPOWProxyHandler::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                        MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    return ParentOf(proxy)->getPropertyDescriptor(cx, proxy, id, desc, flags);
}

bool
JavaScriptParent::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                        MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    ObjectId objId = idOf(proxy);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    ReturnStatus status;
    PPropertyDescriptor result;
    if (!CallGetPropertyDescriptor(objId, idstr, flags, &status, &result))
        return ipcfail(cx);
    if (!ok(cx, status))
        return false;

    return toDescriptor(cx, result, desc);
}

bool
CPOWProxyHandler::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy,
                                           HandleId id, MutableHandle<JSPropertyDescriptor> desc,
                                           unsigned flags)
{
    return ParentOf(proxy)->getOwnPropertyDescriptor(cx, proxy, id, desc, flags);
}

bool
JavaScriptParent::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                           MutableHandle<JSPropertyDescriptor> desc, unsigned flags)
{
    ObjectId objId = idOf(proxy);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    ReturnStatus status;
    PPropertyDescriptor result;
    if (!CallGetOwnPropertyDescriptor(objId, idstr, flags, &status, &result))
        return ipcfail(cx);
    if (!ok(cx, status))
        return false;

    return toDescriptor(cx, result, desc);
}

bool
CPOWProxyHandler::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                 MutableHandle<JSPropertyDescriptor> desc)
{
    return ParentOf(proxy)->defineProperty(cx, proxy, id, desc);
}

bool
JavaScriptParent::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                 MutableHandle<JSPropertyDescriptor> desc)
{
    ObjectId objId = idOf(proxy);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    PPropertyDescriptor descriptor;
    if (!fromDescriptor(cx, desc, &descriptor))
        return false;

    ReturnStatus status;
    if (!CallDefineProperty(objId, idstr, descriptor, &status))
        return ipcfail(cx);

    return ok(cx, status);
}

bool
CPOWProxyHandler::getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    return ParentOf(proxy)->getOwnPropertyNames(cx, proxy, props);
}

bool
JavaScriptParent::getOwnPropertyNames(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    return getPropertyNames(cx, proxy, JSITER_OWNONLY | JSITER_HIDDEN, props);
}

bool
CPOWProxyHandler::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    return ParentOf(proxy)->delete_(cx, proxy, id, bp);
}

bool
JavaScriptParent::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    ObjectId objId = idOf(proxy);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    ReturnStatus status;
    if (!CallDelete(objId, idstr, &status, bp))
        return ipcfail(cx);

    return ok(cx, status);
}

bool
CPOWProxyHandler::enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    return ParentOf(proxy)->enumerate(cx, proxy, props);
}

bool
JavaScriptParent::enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    return getPropertyNames(cx, proxy, 0, props);
}

bool
CPOWProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    return ParentOf(proxy)->has(cx, proxy, id, bp);
}

bool
JavaScriptParent::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    ObjectId objId = idOf(proxy);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    ReturnStatus status;
    if (!CallHas(objId, idstr, &status, bp))
        return ipcfail(cx);

    return ok(cx, status);
}

bool
CPOWProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    return ParentOf(proxy)->hasOwn(cx, proxy, id, bp);
}

bool
JavaScriptParent::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp)
{
    ObjectId objId = idOf(proxy);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    ReturnStatus status;
    if (!CallHasOwn(objId, idstr, &status, bp))
        return ipcfail(cx);

    return !!ok(cx, status);
}

bool
CPOWProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                      HandleId id, MutableHandleValue vp)
{
    return ParentOf(proxy)->get(cx, proxy, receiver, id, vp);
}

bool
JavaScriptParent::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                      HandleId id, MutableHandleValue vp)
{
    ObjectId objId = idOf(proxy);
    ObjectId receiverId = idOf(receiver);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    JSVariant val;
    ReturnStatus status;
    if (!CallGet(objId, receiverId, idstr, &status, &val))
        return ipcfail(cx);

    if (!ok(cx, status))
        return false;

    return toValue(cx, val, vp);
}

bool
CPOWProxyHandler::set(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
                      JS::HandleId id, bool strict, JS::MutableHandleValue vp)
{
    return ParentOf(proxy)->set(cx, proxy, receiver, id, strict, vp);
}

bool
JavaScriptParent::set(JSContext *cx, JS::HandleObject proxy, JS::HandleObject receiver,
                      JS::HandleId id, bool strict, JS::MutableHandleValue vp)
{
    ObjectId objId = idOf(proxy);
    ObjectId receiverId = idOf(receiver);

    nsString idstr;
    if (!convertIdToGeckoString(cx, id, &idstr))
        return false;

    JSVariant val;
    if (!toVariant(cx, vp, &val))
        return false;

    ReturnStatus status;
    JSVariant result;
    if (!CallSet(objId, receiverId, idstr, strict, val, &status, &result))
        return ipcfail(cx);

    if (!ok(cx, status))
        return false;

    return toValue(cx, result, vp);
}

bool
CPOWProxyHandler::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    return ParentOf(proxy)->keys(cx, proxy, props);
}

bool
JavaScriptParent::keys(JSContext *cx, HandleObject proxy, AutoIdVector &props)
{
    return getPropertyNames(cx, proxy, JSITER_OWNONLY, props);
}

bool
CPOWProxyHandler::isExtensible(JSContext *cx, HandleObject proxy, bool *extensible)
{
    return ParentOf(proxy)->isExtensible(cx, proxy, extensible);
}

bool
JavaScriptParent::isExtensible(JSContext *cx, HandleObject proxy, bool *extensible)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    if (!CallIsExtensible(objId, &status, extensible))
        return ipcfail(cx);

    return ok(cx, status);
}

bool
CPOWProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    return ParentOf(proxy)->call(cx, proxy, args);
}

bool
JavaScriptParent::call(JSContext *cx, HandleObject proxy, const CallArgs &args)
{
    ObjectId objId = idOf(proxy);

    InfallibleTArray<JSParam> vals;
    AutoValueVector outobjects(cx);

    RootedValue v(cx);
    for (size_t i = 0; i < args.length() + 2; i++) {
        v = args.base()[i];
        if (v.isObject()) {
            RootedObject obj(cx, &v.toObject());
            if (xpc::IsOutObject(cx, obj)) {
                // Make sure it is not an in-out object.
                bool found;
                if (!JS_HasProperty(cx, obj, "value", &found))
                    return false;
                if (found) {
                    JS_ReportError(cx, "in-out objects cannot be sent via CPOWs yet");
                    return false;
                }

                vals.AppendElement(JSParam(void_t()));
                if (!outobjects.append(ObjectValue(*obj)))
                    return false;
                continue;
            }
        }
        JSVariant val;
        if (!toVariant(cx, v, &val))
            return false;
        vals.AppendElement(JSParam(val));
    }

    JSVariant result;
    ReturnStatus status;
    InfallibleTArray<JSParam> outparams;
    if (!CallCall(objId, vals, &status, &result, &outparams))
        return ipcfail(cx);
    if (!ok(cx, status))
        return false;

    if (outparams.Length() != outobjects.length())
        return ipcfail(cx);

    for (size_t i = 0; i < outparams.Length(); i++) {
        // Don't bother doing anything for outparams that weren't set.
        if (outparams[i].type() == JSParam::Tvoid_t)
            continue;

        // Take the value the child process returned, and set it on the XPC
        // object.
        if (!toValue(cx, outparams[i], &v))
            return false;

        JSObject *obj = &outobjects[i].toObject();
        if (!JS_SetProperty(cx, obj, "value", v))
            return false;
    }

    if (!toValue(cx, result, args.rval()))
        return false;

    return true;
}


bool
CPOWProxyHandler::objectClassIs(HandleObject proxy, js::ESClassValue classValue, JSContext *cx)
{
    return ParentOf(proxy)->objectClassIs(cx, proxy, classValue);
}

bool
JavaScriptParent::objectClassIs(JSContext *cx, HandleObject proxy, js::ESClassValue classValue)
{
    ObjectId objId = idOf(proxy);

    // This function is assumed infallible, so we just return false if the IPC
    // channel fails.
    bool result;
    if (!CallObjectClassIs(objId, classValue, &result))
        return false;

    return result;
}

const char *
CPOWProxyHandler::className(JSContext *cx, HandleObject proxy)
{
    return ParentOf(proxy)->className(cx, proxy);
}

const char *
JavaScriptParent::className(JSContext *cx, HandleObject proxy)
{
    ObjectId objId = idOf(proxy);

    nsString name;
    if (!CallClassName(objId, &name))
        return nullptr;

    return ToNewCString(name);
}

void
CPOWProxyHandler::finalize(JSFreeOp *fop, JSObject *proxy)
{
    ParentOf(proxy)->drop(proxy);
}

void
JavaScriptParent::drop(JSObject *obj)
{
    ObjectId objId = idOf(obj);

    objects_.remove(objId);
    if (!inactive_ && !SendDropObject(objId))
        (void)0;
    decref();
}

bool
JavaScriptParent::init()
{
    if (!JavaScriptShared::init())
        return false;

    return true;
}

bool
JavaScriptParent::makeId(JSContext *cx, JSObject *obj, ObjectId *idp)
{
    obj = js::CheckedUnwrap(obj, false);
    if (!obj || !IsProxy(obj) || GetProxyHandler(obj) != &CPOWProxyHandler::singleton) {
        JS_ReportError(cx, "cannot ipc non-cpow object");
        return false;
    }

    *idp = idOf(obj);
    return true;
}

bool
JavaScriptParent::getPropertyNames(JSContext *cx, HandleObject proxy, uint32_t flags, AutoIdVector &props)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    InfallibleTArray<nsString> names;
    if (!CallGetPropertyNames(objId, flags, &status, &names))
        return ipcfail(cx);
    if (!ok(cx, status))
        return false;

    for (size_t i = 0; i < names.Length(); i++) {
        RootedId name(cx);
        if (!convertGeckoStringToId(cx, names[i], &name))
            return false;
        if (!props.append(name))
            return false;
    }

    return true;
}

JSObject *
JavaScriptParent::unwrap(JSContext *cx, ObjectId objId)
{
    RootedObject obj(cx, findObject(objId));
    if (obj) {
        if (!JS_WrapObject(cx, &obj))
            return nullptr;
        return obj;
    }

    if (objId > MAX_CPOW_IDS) {
        JS_ReportError(cx, "unusable CPOW id");
        return nullptr;
    }

    bool callable = !!(objId & OBJECT_IS_CALLABLE);

    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

    RootedValue v(cx, UndefinedValue());
    ProxyOptions options;
    options.setCallable(callable);
    obj = NewProxyObject(cx,
                         &CPOWProxyHandler::singleton,
                         v,
                         nullptr,
                         global,
                         options);
    if (!obj)
        return nullptr;

    if (!objects_.add(objId, obj))
        return nullptr;

    // Incref once we know the decref will be called.
    incref();

    SetProxyExtra(obj, 0, PrivateValue(this));
    SetProxyExtra(obj, 1, DoubleValue(BitwiseCast<double>(objId)));
    return obj;
}

bool
JavaScriptParent::ipcfail(JSContext *cx)
{
    JS_ReportError(cx, "child process crashed or timedout");
    return false;
}

bool
JavaScriptParent::ok(JSContext *cx, const ReturnStatus &status)
{
    if (status.type() == ReturnStatus::TReturnSuccess)
        return true;

    if (status.type() == ReturnStatus::TReturnStopIteration)
        return JS_ThrowStopIteration(cx);

    RootedValue exn(cx);
    if (!toValue(cx, status.get_ReturnException().exn(), &exn))
        return false;

    JS_SetPendingException(cx, exn);
    return false;
}

void
JavaScriptParent::decref()
{
    refcount_--;
    if (!refcount_)
        delete this;
}

void
JavaScriptParent::incref()
{
    refcount_++;
}

void
JavaScriptParent::destroyFromContent()
{
    inactive_ = true;
    decref();
}

/* static */ bool
JavaScriptParent::IsCPOW(JSObject *obj)
{
    return IsProxy(obj) && GetProxyHandler(obj) == &CPOWProxyHandler::singleton;
}

/* static */ nsresult
JavaScriptParent::InstanceOf(JSObject *obj, const nsID *id, bool *bp)
{
    return ParentOf(obj)->instanceOf(obj, id, bp);
}

nsresult
JavaScriptParent::instanceOf(JSObject *obj, const nsID *id, bool *bp)
{
    ObjectId objId = idOf(obj);

    JSIID iid;
    ConvertID(*id, &iid);

    ReturnStatus status;
    if (!CallInstanceOf(objId, iid, &status, bp))
        return NS_ERROR_UNEXPECTED;

    if (status.type() != ReturnStatus::TReturnSuccess)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

/* static */ bool
JavaScriptParent::DOMInstanceOf(JSObject *obj, int prototypeID, int depth, bool *bp)
{
    return ParentOf(obj)->domInstanceOf(obj, prototypeID, depth, bp);
}

bool
JavaScriptParent::domInstanceOf(JSObject *obj, int prototypeID, int depth, bool *bp)
{
    ObjectId objId = idOf(obj);

    ReturnStatus status;
    if (!CallDOMInstanceOf(objId, prototypeID, depth, &status, bp))
        return false;

    if (status.type() != ReturnStatus::TReturnSuccess)
        return false;

    return true;
}

mozilla::ipc::IProtocol*
JavaScriptParent::CloneProtocol(Channel* aChannel, ProtocolCloneContext* aCtx)
{
    ContentParent *contentParent = aCtx->GetContentParent();
    nsAutoPtr<PJavaScriptParent> actor(contentParent->AllocPJavaScriptParent());
    if (!actor || !contentParent->RecvPJavaScriptConstructor(actor)) {
        return nullptr;
    }
    return actor.forget();
}
