/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WrapperOwner.h"
#include "JavaScriptLogging.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BindingUtils.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"
#include "xpcprivate.h"
#include "CPOWTimer.h"
#include "WrapperFactory.h"

#include "nsIDocShellTreeItem.h"
#include "nsIDOMDocument.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;

struct AuxCPOWData
{
    ObjectId id;
    bool isCallable;
    bool isConstructor;
    bool isDOMObject;

    // The object tag is just some auxilliary information that clients can use
    // however they see fit.
    nsCString objectTag;

    // The class name for WrapperOwner::className, below.
    nsCString className;

    AuxCPOWData(ObjectId id,
                bool isCallable,
                bool isConstructor,
                bool isDOMObject,
                const nsACString& objectTag)
      : id(id),
        isCallable(isCallable),
        isConstructor(isConstructor),
        isDOMObject(isDOMObject),
        objectTag(objectTag)
    {}
};

WrapperOwner::WrapperOwner()
  : inactive_(false)
{
}

static inline AuxCPOWData*
AuxCPOWDataOf(JSObject* obj)
{
    MOZ_ASSERT(IsCPOW(obj));
    return static_cast<AuxCPOWData*>(GetProxyExtra(obj, 1).toPrivate());
}

static inline WrapperOwner*
OwnerOf(JSObject* obj)
{
    MOZ_ASSERT(IsCPOW(obj));
    return reinterpret_cast<WrapperOwner*>(GetProxyExtra(obj, 0).toPrivate());
}

ObjectId
WrapperOwner::idOfUnchecked(JSObject* obj)
{
    MOZ_ASSERT(IsCPOW(obj));

    AuxCPOWData* aux = AuxCPOWDataOf(obj);
    MOZ_ASSERT(!aux->id.isNull());
    return aux->id;
}

ObjectId
WrapperOwner::idOf(JSObject* obj)
{
    ObjectId objId = idOfUnchecked(obj);
    MOZ_ASSERT(hasCPOW(objId, obj));
    return objId;
}

class CPOWProxyHandler : public BaseProxyHandler
{
  public:
    constexpr CPOWProxyHandler()
      : BaseProxyHandler(&family) {}

    virtual bool finalizeInBackground(const Value& priv) const override {
        return false;
    }

    virtual bool getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc) const override;
    virtual bool defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                                Handle<PropertyDescriptor> desc,
                                ObjectOpResult& result) const override;
    virtual bool ownPropertyKeys(JSContext* cx, HandleObject proxy,
                                 AutoIdVector& props) const override;
    virtual bool delete_(JSContext* cx, HandleObject proxy, HandleId id,
                         ObjectOpResult& result) const override;
    virtual bool enumerate(JSContext* cx, HandleObject proxy, MutableHandleObject objp) const override;
    virtual bool preventExtensions(JSContext* cx, HandleObject proxy,
                                   ObjectOpResult& result) const override;
    virtual bool isExtensible(JSContext* cx, HandleObject proxy, bool* extensible) const override;
    virtual bool has(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const override;
    virtual bool get(JSContext* cx, HandleObject proxy, HandleValue receiver,
                     HandleId id, MutableHandleValue vp) const override;
    virtual bool set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue v,
                     JS::HandleValue receiver, JS::ObjectOpResult& result) const override;
    virtual bool call(JSContext* cx, HandleObject proxy, const CallArgs& args) const override;
    virtual bool construct(JSContext* cx, HandleObject proxy, const CallArgs& args) const override;

    virtual bool getPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                       MutableHandle<PropertyDescriptor> desc) const override;
    virtual bool hasOwn(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const override;
    virtual bool getOwnEnumerablePropertyKeys(JSContext* cx, HandleObject proxy,
                                              AutoIdVector& props) const override;
    virtual bool hasInstance(JSContext* cx, HandleObject proxy,
                             MutableHandleValue v, bool* bp) const override;
    virtual bool getBuiltinClass(JSContext* cx, HandleObject obj, js::ESClass* cls) const override;
    virtual bool isArray(JSContext* cx, HandleObject obj,
                         IsArrayAnswer* answer) const override;
    virtual const char* className(JSContext* cx, HandleObject proxy) const override;
    virtual bool regexp_toShared(JSContext* cx, HandleObject proxy,
                                 MutableHandle<RegExpShared*> shared) const override;
    virtual void finalize(JSFreeOp* fop, JSObject* proxy) const override;
    virtual void objectMoved(JSObject* proxy, const JSObject* old) const override;
    virtual bool isCallable(JSObject* obj) const override;
    virtual bool isConstructor(JSObject* obj) const override;
    virtual bool getPrototype(JSContext* cx, HandleObject proxy, MutableHandleObject protop) const override;
    virtual bool getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy, bool* isOrdinary,
                                        MutableHandleObject protop) const override;

    static const char family;
    static const CPOWProxyHandler singleton;
};

const char CPOWProxyHandler::family = 0;
const CPOWProxyHandler CPOWProxyHandler::singleton;

#define FORWARD(call, args)                                             \
    PROFILER_LABEL_FUNC(js::ProfileEntry::Category::JS);                \
    WrapperOwner* owner = OwnerOf(proxy);                               \
    if (!owner->active()) {                                             \
        JS_ReportErrorASCII(cx, "cannot use a CPOW whose process is gone"); \
        return false;                                                   \
    }                                                                   \
    if (!owner->allowMessage(cx)) {                                     \
        return false;                                                   \
    }                                                                   \
    {                                                                   \
        CPOWTimer timer(cx);                                            \
        return owner->call args;                                        \
    }

bool
CPOWProxyHandler::getPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                        MutableHandle<PropertyDescriptor> desc) const
{
    FORWARD(getPropertyDescriptor, (cx, proxy, id, desc));
}

bool
WrapperOwner::getPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                    MutableHandle<PropertyDescriptor> desc)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    ReturnStatus status;
    PPropertyDescriptor result;
    if (!SendGetPropertyDescriptor(objId, idVar, &status, &result))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    return toDescriptor(cx, result, desc);
}

bool
CPOWProxyHandler::getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                           MutableHandle<PropertyDescriptor> desc) const
{
    FORWARD(getOwnPropertyDescriptor, (cx, proxy, id, desc));
}

bool
WrapperOwner::getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                       MutableHandle<PropertyDescriptor> desc)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    ReturnStatus status;
    PPropertyDescriptor result;
    if (!SendGetOwnPropertyDescriptor(objId, idVar, &status, &result))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    return toDescriptor(cx, result, desc);
}

bool
CPOWProxyHandler::defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                                 Handle<PropertyDescriptor> desc,
                                 ObjectOpResult& result) const
{
    FORWARD(defineProperty, (cx, proxy, id, desc, result));
}

bool
WrapperOwner::defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                             Handle<PropertyDescriptor> desc,
                             ObjectOpResult& result)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    PPropertyDescriptor descriptor;
    if (!fromDescriptor(cx, desc, &descriptor))
        return false;

    ReturnStatus status;
    if (!SendDefineProperty(objId, idVar, descriptor, &status))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status, result);
}

bool
CPOWProxyHandler::ownPropertyKeys(JSContext* cx, HandleObject proxy,
                                  AutoIdVector& props) const
{
    FORWARD(ownPropertyKeys, (cx, proxy, props));
}

bool
WrapperOwner::ownPropertyKeys(JSContext* cx, HandleObject proxy, AutoIdVector& props)
{
    return getPropertyKeys(cx, proxy, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
}

bool
CPOWProxyHandler::delete_(JSContext* cx, HandleObject proxy, HandleId id,
                          ObjectOpResult& result) const
{
    FORWARD(delete_, (cx, proxy, id, result));
}

bool
WrapperOwner::delete_(JSContext* cx, HandleObject proxy, HandleId id, ObjectOpResult& result)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    ReturnStatus status;
    if (!SendDelete(objId, idVar, &status))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status, result);
}

bool
CPOWProxyHandler::enumerate(JSContext* cx, HandleObject proxy, MutableHandleObject objp) const
{
    // Using a CPOW for the Iterator would slow down for .. in performance, instead
    // call the base hook, that will use our implementation of getOwnEnumerablePropertyKeys
    // and follow the proto chain.
    return BaseProxyHandler::enumerate(cx, proxy, objp);
}

bool
CPOWProxyHandler::has(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const
{
    FORWARD(has, (cx, proxy, id, bp));
}

bool
WrapperOwner::has(JSContext* cx, HandleObject proxy, HandleId id, bool* bp)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    ReturnStatus status;
    if (!SendHas(objId, idVar, &status, bp))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status);
}

bool
CPOWProxyHandler::hasOwn(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const
{
    FORWARD(hasOwn, (cx, proxy, id, bp));
}

bool
WrapperOwner::hasOwn(JSContext* cx, HandleObject proxy, HandleId id, bool* bp)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    ReturnStatus status;
    if (!SendHasOwn(objId, idVar, &status, bp))
        return ipcfail(cx);

    LOG_STACK();

    return !!ok(cx, status);
}

bool
CPOWProxyHandler::get(JSContext* cx, HandleObject proxy, HandleValue receiver,
                      HandleId id, MutableHandleValue vp) const
{
    FORWARD(get, (cx, proxy, receiver, id, vp));
}

static bool
CPOWDOMQI(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject() || !IsCPOW(&args.thisv().toObject())) {
        JS_ReportErrorASCII(cx, "bad this object passed to special QI");
        return false;
    }

    RootedObject proxy(cx, &args.thisv().toObject());
    FORWARD(DOMQI, (cx, proxy, args));
}

static bool
CPOWToString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());
    RootedValue cpowValue(cx);
    if (!JS_GetProperty(cx, callee, "__cpow__", &cpowValue))
        return false;

    if (!cpowValue.isObject() || !IsCPOW(&cpowValue.toObject())) {
        JS_ReportErrorASCII(cx, "CPOWToString called on an incompatible object");
        return false;
    }

    RootedObject proxy(cx, &cpowValue.toObject());
    FORWARD(toString, (cx, proxy, args));
}

bool
WrapperOwner::toString(JSContext* cx, HandleObject cpow, JS::CallArgs& args)
{
    // Ask the other side to call its toString method. Update the callee so that
    // it points to the CPOW and not to the synthesized CPOWToString function.
    args.setCallee(ObjectValue(*cpow));
    if (!callOrConstruct(cx, cpow, args, false))
        return false;

    if (!args.rval().isString())
        return true;

    RootedString cpowResult(cx, args.rval().toString());
    nsAutoJSString toStringResult;
    if (!toStringResult.init(cx, cpowResult))
        return false;

    // We don't want to wrap toString() results for things like the location
    // object, where toString() is supposed to return a URL and nothing else.
    nsAutoString result;
    if (toStringResult[0] == '[') {
        result.AppendLiteral("[object CPOW ");
        result += toStringResult;
        result.AppendLiteral("]");
    } else {
        result += toStringResult;
    }

    JSString* str = JS_NewUCStringCopyN(cx, result.get(), result.Length());
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

bool
WrapperOwner::DOMQI(JSContext* cx, JS::HandleObject proxy, JS::CallArgs& args)
{
    // Someone's calling us, handle nsISupports specially to avoid unnecessary
    // CPOW traffic.
    HandleValue id = args[0];
    if (id.isObject()) {
        RootedObject idobj(cx, &id.toObject());
        nsCOMPtr<nsIJSID> jsid;

        nsresult rv = UnwrapArg<nsIJSID>(cx, idobj, getter_AddRefs(jsid));
        if (NS_SUCCEEDED(rv)) {
            MOZ_ASSERT(jsid, "bad wrapJS");
            const nsID* idptr = jsid->GetID();
            if (idptr->Equals(NS_GET_IID(nsISupports))) {
                args.rval().set(args.thisv());
                return true;
            }

            // Webidl-implemented DOM objects never have nsIClassInfo.
            if (idptr->Equals(NS_GET_IID(nsIClassInfo)))
                return Throw(cx, NS_ERROR_NO_INTERFACE);
        }
    }

    // It wasn't nsISupports, call into the other process to do the QI for us
    // (since we don't know what other interfaces our object supports). Note
    // that we have to use JS_GetPropertyDescriptor here to avoid infinite
    // recursion back into CPOWDOMQI via WrapperOwner::get().
    // We could stash the actual QI function on our own function object to avoid
    // if we're called multiple times, but since we're transient, there's no
    // point right now.
    JS::Rooted<PropertyDescriptor> propDesc(cx);
    if (!JS_GetPropertyDescriptor(cx, proxy, "QueryInterface", &propDesc))
        return false;

    if (!propDesc.value().isObject()) {
        MOZ_ASSERT_UNREACHABLE("We didn't get QueryInterface off a node");
        return Throw(cx, NS_ERROR_UNEXPECTED);
    }
    return JS_CallFunctionValue(cx, proxy, propDesc.value(), args, args.rval());
}

bool
WrapperOwner::get(JSContext* cx, HandleObject proxy, HandleValue receiver,
                  HandleId id, MutableHandleValue vp)
{
    ObjectId objId = idOf(proxy);

    JSVariant receiverVar;
    if (!toVariant(cx, receiver, &receiverVar))
        return false;

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    AuxCPOWData* data = AuxCPOWDataOf(proxy);
    if (data->isDOMObject &&
        idVar.type() == JSIDVariant::TnsString &&
        idVar.get_nsString().EqualsLiteral("QueryInterface"))
    {
        // Handle QueryInterface on DOM Objects specially since we can assume
        // certain things about their implementation.
        RootedFunction qi(cx, JS_NewFunction(cx, CPOWDOMQI, 1, 0,
                                             "QueryInterface"));
        if (!qi)
            return false;

        vp.set(ObjectValue(*JS_GetFunctionObject(qi)));
        return true;
    }

    JSVariant val;
    ReturnStatus status;
    if (!SendGet(objId, receiverVar, idVar, &status, &val))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    if (!fromVariant(cx, val, vp))
        return false;

    if (idVar.type() == JSIDVariant::TnsString &&
        idVar.get_nsString().EqualsLiteral("toString")) {
        RootedFunction toString(cx, JS_NewFunction(cx, CPOWToString, 0, 0,
                                                   "toString"));
        if (!toString)
            return false;

        RootedObject toStringObj(cx, JS_GetFunctionObject(toString));

        if (!JS_DefineProperty(cx, toStringObj, "__cpow__", vp, JSPROP_PERMANENT | JSPROP_READONLY))
            return false;

        vp.set(ObjectValue(*toStringObj));
    }

    return true;
}

bool
CPOWProxyHandler::set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue v,
                      JS::HandleValue receiver, JS::ObjectOpResult& result) const
{
    FORWARD(set, (cx, proxy, id, v, receiver, result));
}

bool
WrapperOwner::set(JSContext* cx, JS::HandleObject proxy, JS::HandleId id, JS::HandleValue v,
                  JS::HandleValue receiver, JS::ObjectOpResult& result)
{
    ObjectId objId = idOf(proxy);

    JSIDVariant idVar;
    if (!toJSIDVariant(cx, id, &idVar))
        return false;

    JSVariant val;
    if (!toVariant(cx, v, &val))
        return false;

    JSVariant receiverVar;
    if (!toVariant(cx, receiver, &receiverVar))
        return false;

    ReturnStatus status;
    if (!SendSet(objId, idVar, val, receiverVar, &status))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status, result);
}

bool
CPOWProxyHandler::getOwnEnumerablePropertyKeys(JSContext* cx, HandleObject proxy,
                                               AutoIdVector& props) const
{
    FORWARD(getOwnEnumerablePropertyKeys, (cx, proxy, props));
}

bool
WrapperOwner::getOwnEnumerablePropertyKeys(JSContext* cx, HandleObject proxy, AutoIdVector& props)
{
    return getPropertyKeys(cx, proxy, JSITER_OWNONLY, props);
}

bool
CPOWProxyHandler::preventExtensions(JSContext* cx, HandleObject proxy, ObjectOpResult& result) const
{
    FORWARD(preventExtensions, (cx, proxy, result));
}

bool
WrapperOwner::preventExtensions(JSContext* cx, HandleObject proxy, ObjectOpResult& result)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    if (!SendPreventExtensions(objId, &status))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status, result);
}

bool
CPOWProxyHandler::isExtensible(JSContext* cx, HandleObject proxy, bool* extensible) const
{
    FORWARD(isExtensible, (cx, proxy, extensible));
}

bool
WrapperOwner::isExtensible(JSContext* cx, HandleObject proxy, bool* extensible)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    if (!SendIsExtensible(objId, &status, extensible))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status);
}

bool
CPOWProxyHandler::call(JSContext* cx, HandleObject proxy, const CallArgs& args) const
{
    FORWARD(callOrConstruct, (cx, proxy, args, false));
}

bool
CPOWProxyHandler::construct(JSContext* cx, HandleObject proxy, const CallArgs& args) const
{
    FORWARD(callOrConstruct, (cx, proxy, args, true));
}

bool
WrapperOwner::callOrConstruct(JSContext* cx, HandleObject proxy, const CallArgs& args,
                              bool construct)
{
    ObjectId objId = idOf(proxy);

    InfallibleTArray<JSParam> vals;
    AutoValueVector outobjects(cx);

    RootedValue v(cx);
    for (size_t i = 0; i < args.length() + 2; i++) {
        // The |this| value for constructors is a magic value that we won't be
        // able to convert, so skip it.
        if (i == 1 && construct)
            v = UndefinedValue();
        else
            v = args.base()[i];
        if (v.isObject()) {
            RootedObject obj(cx, &v.toObject());
            if (xpc::IsOutObject(cx, obj)) {
                // Make sure it is not an in-out object.
                bool found;
                if (!JS_HasProperty(cx, obj, "value", &found))
                    return false;
                if (found) {
                    JS_ReportErrorASCII(cx, "in-out objects cannot be sent via CPOWs yet");
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
    if (!SendCallOrConstruct(objId, vals, construct, &status, &result, &outparams))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    if (outparams.Length() != outobjects.length())
        return ipcfail(cx);

    RootedObject obj(cx);
    for (size_t i = 0; i < outparams.Length(); i++) {
        // Don't bother doing anything for outparams that weren't set.
        if (outparams[i].type() == JSParam::Tvoid_t)
            continue;

        // Take the value the child process returned, and set it on the XPC
        // object.
        if (!fromVariant(cx, outparams[i], &v))
            return false;

        obj = &outobjects[i].toObject();
        if (!JS_SetProperty(cx, obj, "value", v))
            return false;
    }

    if (!fromVariant(cx, result, args.rval()))
        return false;

    return true;
}

bool
CPOWProxyHandler::hasInstance(JSContext* cx, HandleObject proxy, MutableHandleValue v, bool* bp) const
{
    FORWARD(hasInstance, (cx, proxy, v, bp));
}

bool
WrapperOwner::hasInstance(JSContext* cx, HandleObject proxy, MutableHandleValue v, bool* bp)
{
    ObjectId objId = idOf(proxy);

    JSVariant vVar;
    if (!toVariant(cx, v, &vVar))
        return false;

    ReturnStatus status;
    if (!SendHasInstance(objId, vVar, &status, bp))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status);
}

bool
CPOWProxyHandler::getBuiltinClass(JSContext* cx, HandleObject proxy, ESClass* cls) const
{
    FORWARD(getBuiltinClass, (cx, proxy, cls));
}

bool
WrapperOwner::getBuiltinClass(JSContext* cx, HandleObject proxy, ESClass* cls)
{
    ObjectId objId = idOf(proxy);

    uint32_t classValue = uint32_t(ESClass::Other);
    ReturnStatus status;
    if (!SendGetBuiltinClass(objId, &status, &classValue))
        return ipcfail(cx);
    *cls = ESClass(classValue);

    LOG_STACK();

    return ok(cx, status);
}

bool
CPOWProxyHandler::isArray(JSContext* cx, HandleObject proxy,
                          IsArrayAnswer* answer) const
{
    FORWARD(isArray, (cx, proxy, answer));
}

bool
WrapperOwner::isArray(JSContext* cx, HandleObject proxy, IsArrayAnswer* answer)
{
    ObjectId objId = idOf(proxy);

    uint32_t ans;
    ReturnStatus status;
    if (!SendIsArray(objId, &status, &ans))
        return ipcfail(cx);

    LOG_STACK();

    *answer = IsArrayAnswer(ans);
    MOZ_ASSERT(*answer == IsArrayAnswer::Array ||
               *answer == IsArrayAnswer::NotArray ||
               *answer == IsArrayAnswer::RevokedProxy);

    return ok(cx, status);
}

const char*
CPOWProxyHandler::className(JSContext* cx, HandleObject proxy) const
{
    WrapperOwner* parent = OwnerOf(proxy);
    if (!parent->active())
        return "<dead CPOW>";
    return parent->className(cx, proxy);
}

const char*
WrapperOwner::className(JSContext* cx, HandleObject proxy)
{
    AuxCPOWData* data = AuxCPOWDataOf(proxy);
    if (data->className.IsEmpty()) {
        ObjectId objId = idOf(proxy);

        if (!SendClassName(objId, &data->className))
            return "<error>";

        LOG_STACK();
    }

    return data->className.get();
}

bool
CPOWProxyHandler::getPrototype(JSContext* cx, HandleObject proxy, MutableHandleObject objp) const
{
    FORWARD(getPrototype, (cx, proxy, objp));
}

bool
WrapperOwner::getPrototype(JSContext* cx, HandleObject proxy, MutableHandleObject objp)
{
    ObjectId objId = idOf(proxy);

    ObjectOrNullVariant val;
    ReturnStatus status;
    if (!SendGetPrototype(objId, &status, &val))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    objp.set(fromObjectOrNullVariant(cx, val));

    return true;
}

bool
CPOWProxyHandler::getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy, bool* isOrdinary,
                                         MutableHandleObject objp) const
{
    FORWARD(getPrototypeIfOrdinary, (cx, proxy, isOrdinary, objp));
}

bool
WrapperOwner::getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy, bool* isOrdinary,
                                     MutableHandleObject objp)
{
    ObjectId objId = idOf(proxy);

    ObjectOrNullVariant val;
    ReturnStatus status;
    if (!SendGetPrototypeIfOrdinary(objId, &status, isOrdinary, &val))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    objp.set(fromObjectOrNullVariant(cx, val));

    return true;
}

bool
CPOWProxyHandler::regexp_toShared(JSContext* cx, HandleObject proxy,
                                  MutableHandle<RegExpShared*> shared) const
{
    FORWARD(regexp_toShared, (cx, proxy, shared));
}

bool
WrapperOwner::regexp_toShared(JSContext* cx, HandleObject proxy, MutableHandle<RegExpShared*> shared)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    nsString source;
    unsigned flags = 0;
    if (!SendRegExpToShared(objId, &status, &source, &flags))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    RootedObject regexp(cx);
    regexp = JS_NewUCRegExpObject(cx, source.get(), source.Length(), flags);
    if (!regexp)
        return false;

    return js::RegExpToSharedNonInline(cx, regexp, shared);
}

void
CPOWProxyHandler::finalize(JSFreeOp* fop, JSObject* proxy) const
{
    AuxCPOWData* aux = AuxCPOWDataOf(proxy);

    OwnerOf(proxy)->drop(proxy);

    if (aux)
        delete aux;
}

void
CPOWProxyHandler::objectMoved(JSObject* proxy, const JSObject* old) const
{
    OwnerOf(proxy)->updatePointer(proxy, old);
}

bool
CPOWProxyHandler::isCallable(JSObject* proxy) const
{
    AuxCPOWData* aux = AuxCPOWDataOf(proxy);
    return aux->isCallable;
}

bool
CPOWProxyHandler::isConstructor(JSObject* proxy) const
{
    AuxCPOWData* aux = AuxCPOWDataOf(proxy);
    return aux->isConstructor;
}

void
WrapperOwner::drop(JSObject* obj)
{
    // The association may have already been swept from the table but if it's
    // there then remove it.
    ObjectId objId = idOfUnchecked(obj);
    if (cpows_.findPreserveColor(objId) == obj)
        cpows_.remove(objId);

    if (active())
        Unused << SendDropObject(objId);
    decref();
}

void
WrapperOwner::updatePointer(JSObject* obj, const JSObject* old)
{
    ObjectId objId = idOfUnchecked(obj);
    MOZ_ASSERT(hasCPOW(objId, old));
    cpows_.add(objId, obj);
}

bool
WrapperOwner::init()
{
    if (!JavaScriptShared::init())
        return false;

    return true;
}

bool
WrapperOwner::getPropertyKeys(JSContext* cx, HandleObject proxy, uint32_t flags, AutoIdVector& props)
{
    ObjectId objId = idOf(proxy);

    ReturnStatus status;
    InfallibleTArray<JSIDVariant> ids;
    if (!SendGetPropertyKeys(objId, flags, &status, &ids))
        return ipcfail(cx);

    LOG_STACK();

    if (!ok(cx, status))
        return false;

    for (size_t i = 0; i < ids.Length(); i++) {
        RootedId id(cx);
        if (!fromJSIDVariant(cx, ids[i], &id))
            return false;
        if (!props.append(id))
            return false;
    }

    return true;
}

namespace mozilla {
namespace jsipc {

bool
IsCPOW(JSObject* obj)
{
    return IsProxy(obj) && GetProxyHandler(obj) == &CPOWProxyHandler::singleton;
}

bool
IsWrappedCPOW(JSObject* obj)
{
    JSObject* unwrapped = js::UncheckedUnwrap(obj, true);
    if (!unwrapped)
        return false;
    return IsCPOW(unwrapped);
}

void
GetWrappedCPOWTag(JSObject* obj, nsACString& out)
{
    JSObject* unwrapped = js::UncheckedUnwrap(obj, true);
    MOZ_ASSERT(IsCPOW(unwrapped));

    AuxCPOWData* aux = AuxCPOWDataOf(unwrapped);
    if (aux)
        out = aux->objectTag;
}

nsresult
InstanceOf(JSObject* proxy, const nsID* id, bool* bp)
{
    WrapperOwner* parent = OwnerOf(proxy);
    if (!parent->active())
        return NS_ERROR_UNEXPECTED;
    return parent->instanceOf(proxy, id, bp);
}

bool
DOMInstanceOf(JSContext* cx, JSObject* proxyArg, int prototypeID, int depth, bool* bp)
{
    RootedObject proxy(cx, proxyArg);
    FORWARD(domInstanceOf, (cx, proxy, prototypeID, depth, bp));
}

} /* namespace jsipc */
} /* namespace mozilla */

nsresult
WrapperOwner::instanceOf(JSObject* obj, const nsID* id, bool* bp)
{
    ObjectId objId = idOf(obj);

    JSIID iid;
    ConvertID(*id, &iid);

    ReturnStatus status;
    if (!SendInstanceOf(objId, iid, &status, bp))
        return NS_ERROR_UNEXPECTED;

    if (status.type() != ReturnStatus::TReturnSuccess)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

bool
WrapperOwner::domInstanceOf(JSContext* cx, JSObject* obj, int prototypeID, int depth, bool* bp)
{
    ObjectId objId = idOf(obj);

    ReturnStatus status;
    if (!SendDOMInstanceOf(objId, prototypeID, depth, &status, bp))
        return ipcfail(cx);

    LOG_STACK();

    return ok(cx, status);
}

void
WrapperOwner::ActorDestroy(ActorDestroyReason why)
{
    inactive_ = true;

    objects_.clear();
    unwaivedObjectIds_.clear();
    waivedObjectIds_.clear();
}

bool
WrapperOwner::ipcfail(JSContext* cx)
{
    JS_ReportErrorASCII(cx, "cross-process JS call failed");
    return false;
}

bool
WrapperOwner::ok(JSContext* cx, const ReturnStatus& status)
{
    if (status.type() == ReturnStatus::TReturnSuccess)
        return true;

    if (status.type() == ReturnStatus::TReturnStopIteration)
        return JS_ThrowStopIteration(cx);

    if (status.type() == ReturnStatus::TReturnDeadCPOW) {
        JS_ReportErrorASCII(cx, "operation not possible on dead CPOW");
        return false;
    }

    RootedValue exn(cx);
    if (!fromVariant(cx, status.get_ReturnException().exn(), &exn))
        return false;

    JS_SetPendingException(cx, exn);
    return false;
}

bool
WrapperOwner::ok(JSContext* cx, const ReturnStatus& status, ObjectOpResult& result)
{
    if (status.type() == ReturnStatus::TReturnObjectOpResult)
        return result.fail(status.get_ReturnObjectOpResult().code());
    if (!ok(cx, status))
        return false;
    return result.succeed();
}

// CPOWs can have a tag string attached to them, originating in the local
// process from this function.  It's sent with the CPOW to the remote process,
// where it can be fetched with Components.utils.getCrossProcessWrapperTag.
static nsCString
GetRemoteObjectTag(JS::Handle<JSObject*> obj)
{
    if (nsCOMPtr<nsISupports> supports = xpc::UnwrapReflectorToISupports(obj)) {
        nsCOMPtr<nsIDocShellTreeItem> treeItem(do_QueryInterface(supports));
        if (treeItem)
            return NS_LITERAL_CSTRING("ContentDocShellTreeItem");

        nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(supports));
        if (doc)
            return NS_LITERAL_CSTRING("ContentDocument");
    }

    return NS_LITERAL_CSTRING("generic");
}

static RemoteObject
MakeRemoteObject(JSContext* cx, ObjectId id, HandleObject obj)
{
    return RemoteObject(id.serialize(),
                        JS::IsCallable(obj),
                        JS::IsConstructor(obj),
                        dom::IsDOMObject(obj),
                        GetRemoteObjectTag(obj));
}

bool
WrapperOwner::toObjectVariant(JSContext* cx, JSObject* objArg, ObjectVariant* objVarp)
{
    RootedObject obj(cx, objArg);
    MOZ_ASSERT(obj);

    // We always save objects unwrapped in the CPOW table. If we stored
    // wrappers, then the wrapper might be GCed while the target remained alive.
    // Whenever operating on an object that comes from the table, we wrap it
    // in findObjectById.
    unsigned wrapperFlags = 0;
    obj = js::UncheckedUnwrap(obj, true, &wrapperFlags);
    if (obj && IsCPOW(obj) && OwnerOf(obj) == this) {
        *objVarp = LocalObject(idOf(obj).serialize());
        return true;
    }
    bool waiveXray = wrapperFlags & xpc::WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG;

    ObjectId id = objectIdMap(waiveXray).find(obj);
    if (!id.isNull()) {
        MOZ_ASSERT(id.hasXrayWaiver() == waiveXray);
        *objVarp = MakeRemoteObject(cx, id, obj);
        return true;
    }

    // Need to call PreserveWrapper on |obj| in case it's a reflector.
    // FIXME: What if it's an XPCWrappedNative?
    if (mozilla::dom::IsDOMObject(obj))
        mozilla::dom::TryPreserveWrapper(obj);

    id = ObjectId(nextSerialNumber_++, waiveXray);
    if (!objects_.add(id, obj))
        return false;
    if (!objectIdMap(waiveXray).add(cx, obj, id))
        return false;

    *objVarp = MakeRemoteObject(cx, id, obj);
    return true;
}

JSObject*
WrapperOwner::fromObjectVariant(JSContext* cx, const ObjectVariant& objVar)
{
    if (objVar.type() == ObjectVariant::TRemoteObject) {
        return fromRemoteObjectVariant(cx, objVar.get_RemoteObject());
    } else {
        return fromLocalObjectVariant(cx, objVar.get_LocalObject());
    }
}

JSObject*
WrapperOwner::fromRemoteObjectVariant(JSContext* cx, const RemoteObject& objVar)
{
    ObjectId objId = ObjectId::deserialize(objVar.serializedId());
    RootedObject obj(cx, findCPOWById(objId));
    if (!obj) {

        // All CPOWs live in the privileged junk scope.
        RootedObject junkScope(cx, xpc::PrivilegedJunkScope());
        JSAutoCompartment ac(cx, junkScope);
        RootedValue v(cx, UndefinedValue());
        // We need to setLazyProto for the getPrototype/getPrototypeIfOrdinary
        // hooks.
        ProxyOptions options;
        options.setLazyProto(true);
        obj = NewProxyObject(cx,
                             &CPOWProxyHandler::singleton,
                             v,
                             nullptr,
                             options);
        if (!obj)
            return nullptr;

        if (!cpows_.add(objId, obj))
            return nullptr;

        nextCPOWNumber_ = objId.serialNumber() + 1;

        // Incref once we know the decref will be called.
        incref();

        AuxCPOWData* aux = new AuxCPOWData(objId,
                                           objVar.isCallable(),
                                           objVar.isConstructor(),
                                           objVar.isDOMObject(),
                                           objVar.objectTag());

        SetProxyExtra(obj, 0, PrivateValue(this));
        SetProxyExtra(obj, 1, PrivateValue(aux));
    }

    if (!JS_WrapObject(cx, &obj))
        return nullptr;
    return obj;
}

JSObject*
WrapperOwner::fromLocalObjectVariant(JSContext* cx, const LocalObject& objVar)
{
    ObjectId id = ObjectId::deserialize(objVar.serializedId());
    Rooted<JSObject*> obj(cx, findObjectById(cx, id));
    if (!obj)
        return nullptr;
    if (!JS_WrapObject(cx, &obj))
        return nullptr;
    return obj;
}
