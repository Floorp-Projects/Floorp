/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WrapperAnswer.h"
#include "JavaScriptLogging.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "xpcprivate.h"
#include "jsfriendapi.h"

using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;

// Note - Using AutoJSAPI (rather than AutoEntryScript) for a trap means
// that we don't expect it to run script. For most of these traps that will only
// happen if the target is a scripted proxy, which is probably something that we
// don't want to support over CPOWs. When enough code is fixed up, the long-term
// plan is to have the JS engine throw if it encounters script when it isn't
// expecting it.
using mozilla::dom::AutoJSAPI;
using mozilla::dom::AutoEntryScript;

bool
WrapperAnswer::fail(JSContext *cx, ReturnStatus *rs)
{
    // By default, we set |undefined| unless we can get a more meaningful
    // exception.
    *rs = ReturnStatus(ReturnException(JSVariant(UndefinedVariant())));

    // Note we always return true from this function, since this propagates
    // to the IPC code, and we don't want a JS failure to cause the death
    // of the child process.

    RootedValue exn(cx);
    if (!JS_GetPendingException(cx, &exn))
        return true;

    // If we don't clear the pending exception, JS will try to wrap it as it
    // leaves the current compartment. Since there is no previous compartment,
    // that would crash.
    JS_ClearPendingException(cx);

    if (JS_IsStopIteration(exn)) {
        *rs = ReturnStatus(ReturnStopIteration());
        return true;
    }

    // If this fails, we still don't want to exit. Just return an invalid
    // exception.
    (void) toVariant(cx, exn, &rs->get_ReturnException().exn());
    return true;
}

bool
WrapperAnswer::ok(ReturnStatus *rs)
{
    *rs = ReturnStatus(ReturnSuccess());
    return true;
}

bool
WrapperAnswer::RecvPreventExtensions(const ObjectId &objId, ReturnStatus *rs,
                                     bool *succeeded)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    *succeeded = false;
    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    if (!JS_PreventExtensions(cx, obj, succeeded))
        return fail(cx, rs);

    LOG("%s.preventExtensions()", ReceiverObj(objId));

    return ok(rs);
}

static void
EmptyDesc(PPropertyDescriptor *desc)
{
    desc->obj() = LocalObject(0);
    desc->attrs() = 0;
    desc->value() = UndefinedVariant();
    desc->getter() = 0;
    desc->setter() = 0;
}

bool
WrapperAnswer::RecvGetPropertyDescriptor(const ObjectId &objId, const JSIDVariant &idVar,
                                         ReturnStatus *rs, PPropertyDescriptor *out)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    EmptyDesc(out);

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.getPropertyDescriptor(%s)", ReceiverObj(objId), Identifier(idVar));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, obj, id, &desc))
        return fail(cx, rs);

    if (!fromDescriptor(cx, desc, out))
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvGetOwnPropertyDescriptor(const ObjectId &objId, const JSIDVariant &idVar,
                                            ReturnStatus *rs, PPropertyDescriptor *out)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    EmptyDesc(out);

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.getOwnPropertyDescriptor(%s)", ReceiverObj(objId), Identifier(idVar));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, obj, id, &desc))
        return fail(cx, rs);

    if (!fromDescriptor(cx, desc, out))
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvDefineProperty(const ObjectId &objId, const JSIDVariant &idVar,
                                  const PPropertyDescriptor &descriptor, ReturnStatus *rs)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("define %s[%s]", ReceiverObj(objId), Identifier(idVar));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!toDescriptor(cx, descriptor, &desc))
        return fail(cx, rs);

    bool ignored;
    if (!js_DefineOwnProperty(cx, obj, id, desc, &ignored))
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvDelete(const ObjectId &objId, const JSIDVariant &idVar, ReturnStatus *rs,
                          bool *success)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    *success = false;

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("delete %s[%s]", ReceiverObj(objId), Identifier(idVar));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    if (!JS_DeletePropertyById2(cx, obj, id, success))
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvHas(const ObjectId &objId, const JSIDVariant &idVar, ReturnStatus *rs, bool *bp)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    *bp = false;

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.has(%s)", ReceiverObj(objId), Identifier(idVar));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    bool found;
    if (!JS_HasPropertyById(cx, obj, id, &found))
        return fail(cx, rs);
    *bp = !!found;

    return ok(rs);
}

bool
WrapperAnswer::RecvHasOwn(const ObjectId &objId, const JSIDVariant &idVar, ReturnStatus *rs,
                          bool *bp)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    *bp = false;

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.hasOwn(%s)", ReceiverObj(objId), Identifier(idVar));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, obj, id, &desc))
        return fail(cx, rs);
    *bp = (desc.object() == obj);

    return ok(rs);
}

bool
WrapperAnswer::RecvGet(const ObjectId &objId, const ObjectVariant &receiverVar,
                       const JSIDVariant &idVar, ReturnStatus *rs, JSVariant *result)
{
    // We may run scripted getters.
    AutoEntryScript aes(xpc::NativeGlobal(scopeForTargetObjects()));
    JSContext *cx = aes.cx();

    // The outparam will be written to the buffer, so it must be set even if
    // the parent won't read it.
    *result = UndefinedVariant();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    RootedObject receiver(cx, fromObjectVariant(cx, receiverVar));
    if (!receiver)
        return fail(cx, rs);

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    JS::RootedValue val(cx);
    if (!JS_ForwardGetPropertyTo(cx, obj, id, receiver, &val))
        return fail(cx, rs);

    if (!toVariant(cx, val, result))
        return fail(cx, rs);

    LOG("get %s.%s = %s", ReceiverObj(objId), Identifier(idVar), OutVariant(*result));

    return ok(rs);
}

bool
WrapperAnswer::RecvSet(const ObjectId &objId, const ObjectVariant &receiverVar,
                       const JSIDVariant &idVar, const bool &strict, const JSVariant &value,
                       ReturnStatus *rs, JSVariant *result)
{
    // We may run scripted setters.
    AutoEntryScript aes(xpc::NativeGlobal(scopeForTargetObjects()));
    JSContext *cx = aes.cx();

    // The outparam will be written to the buffer, so it must be set even if
    // the parent won't read it.
    *result = UndefinedVariant();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    RootedObject receiver(cx, fromObjectVariant(cx, receiverVar));
    if (!receiver)
        return fail(cx, rs);

    LOG("set %s[%s] = %s", ReceiverObj(objId), Identifier(idVar), InVariant(value));

    RootedId id(cx);
    if (!fromJSIDVariant(cx, idVar, &id))
        return fail(cx, rs);

    MOZ_ASSERT(obj == receiver);

    RootedValue val(cx);
    if (!fromVariant(cx, value, &val))
        return fail(cx, rs);

    if (!JS_SetPropertyById(cx, obj, id, val))
        return fail(cx, rs);

    if (!toVariant(cx, val, result))
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvIsExtensible(const ObjectId &objId, ReturnStatus *rs, bool *result)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    *result = false;

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.isExtensible()", ReceiverObj(objId));

    bool extensible;
    if (!JS_IsExtensible(cx, obj, &extensible))
        return fail(cx, rs);

    *result = !!extensible;
    return ok(rs);
}

bool
WrapperAnswer::RecvCallOrConstruct(const ObjectId &objId,
                                   InfallibleTArray<JSParam> &&argv,
                                   const bool &construct,
                                   ReturnStatus *rs,
                                   JSVariant *result,
                                   nsTArray<JSParam> *outparams)
{
    AutoEntryScript aes(xpc::NativeGlobal(scopeForTargetObjects()));
    JSContext *cx = aes.cx();

    // The outparam will be written to the buffer, so it must be set even if
    // the parent won't read it.
    *result = UndefinedVariant();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    MOZ_ASSERT(argv.Length() >= 2);

    RootedValue objv(cx);
    if (!fromVariant(cx, argv[0], &objv))
        return fail(cx, rs);

    *result = JSVariant(UndefinedVariant());

    AutoValueVector vals(cx);
    AutoValueVector outobjects(cx);
    for (size_t i = 0; i < argv.Length(); i++) {
        if (argv[i].type() == JSParam::Tvoid_t) {
            // This is an outparam.
            RootedObject obj(cx, xpc::NewOutObject(cx));
            if (!obj)
                return fail(cx, rs);
            if (!outobjects.append(ObjectValue(*obj)))
                return fail(cx, rs);
            if (!vals.append(ObjectValue(*obj)))
                return fail(cx, rs);
        } else {
            RootedValue v(cx);
            if (!fromVariant(cx, argv[i].get_JSVariant(), &v))
                return fail(cx, rs);
            if (!vals.append(v))
                return fail(cx, rs);
        }
    }

    RootedValue rval(cx);
    {
        AutoSaveContextOptions asco(cx);
        ContextOptionsRef(cx).setDontReportUncaught(true);

        HandleValueArray args = HandleValueArray::subarray(vals, 2, vals.length() - 2);
        bool success;
        if (construct)
            success = JS::Construct(cx, vals[0], args, &rval);
        else
            success = JS::Call(cx, vals[1], vals[0], args, &rval);
        if (!success)
            return fail(cx, rs);
    }

    if (!toVariant(cx, rval, result))
        return fail(cx, rs);

    // Prefill everything with a dummy jsval.
    for (size_t i = 0; i < outobjects.length(); i++)
        outparams->AppendElement(JSParam(void_t()));

    // Go through each argument that was an outparam, retrieve the "value"
    // field, and add it to a temporary list. We need to do this separately
    // because the outparams vector is not rooted.
    vals.clear();
    for (size_t i = 0; i < outobjects.length(); i++) {
        RootedObject obj(cx, &outobjects[i].toObject());

        RootedValue v(cx);
        bool found;
        if (JS_HasProperty(cx, obj, "value", &found)) {
            if (!JS_GetProperty(cx, obj, "value", &v))
                return fail(cx, rs);
        } else {
            v = UndefinedValue();
        }
        if (!vals.append(v))
            return fail(cx, rs);
    }

    // Copy the outparams. If any outparam is already set to a void_t, we
    // treat this as the outparam never having been set.
    for (size_t i = 0; i < vals.length(); i++) {
        JSVariant variant;
        if (!toVariant(cx, vals[i], &variant))
            return fail(cx, rs);
        outparams->ReplaceElementAt(i, JSParam(variant));
    }

    LOG("%s.call(%s) = %s", ReceiverObj(objId), argv, OutVariant(*result));

    return ok(rs);
}

bool
WrapperAnswer::RecvHasInstance(const ObjectId &objId, const JSVariant &vVar, ReturnStatus *rs, bool *bp)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.hasInstance(%s)", ReceiverObj(objId), InVariant(vVar));

    RootedValue val(cx);
    if (!fromVariant(cx, vVar, &val))
        return fail(cx, rs);

    if (!JS_HasInstance(cx, obj, val, bp))
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvObjectClassIs(const ObjectId &objId, const uint32_t &classValue,
                                 bool *result)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj) {
        // This is very unfortunate, but we have no choice.
        *result = false;
        return true;
    }

    LOG("%s.objectClassIs()", ReceiverObj(objId));

    *result = js_ObjectClassIs(cx, obj, (js::ESClassValue)classValue);
    return true;
}

bool
WrapperAnswer::RecvClassName(const ObjectId &objId, nsString *name)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj) {
        // This is very unfortunate, but we have no choice.
        return "<dead CPOW>";
    }

    LOG("%s.className()", ReceiverObj(objId));

    *name = NS_ConvertASCIItoUTF16(js_ObjectClassName(cx, obj));
    return true;
}

bool
WrapperAnswer::RecvGetPrototypeOf(const ObjectId &objId, ReturnStatus *rs, ObjectOrNullVariant *result)
{
    *result = NullVariant();

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    JS::RootedObject proto(cx);
    if (!JS_GetPrototype(cx, obj, &proto))
        return fail(cx, rs);

    if (!toObjectOrNullVariant(cx, proto, result))
        return fail(cx, rs);

    LOG("getPrototypeOf(%s)", ReceiverObj(objId));

    return ok(rs);
}

bool
WrapperAnswer::RecvRegExpToShared(const ObjectId &objId, ReturnStatus *rs,
                                  nsString *source, uint32_t *flags)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    MOZ_RELEASE_ASSERT(JS_ObjectIsRegExp(cx, obj));
    RootedString sourceJSStr(cx, JS_GetRegExpSource(cx, obj));
    if (!sourceJSStr)
        return fail(cx, rs);
    nsAutoJSString sourceStr;
    if (!sourceStr.init(cx, sourceJSStr))
        return fail(cx, rs);
    source->Assign(sourceStr);

    *flags = JS_GetRegExpFlags(cx, obj);

    return ok(rs);
}

bool
WrapperAnswer::RecvGetPropertyKeys(const ObjectId &objId, const uint32_t &flags,
                                   ReturnStatus *rs, nsTArray<JSIDVariant> *ids)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.getPropertyKeys()", ReceiverObj(objId));

    AutoIdVector props(cx);
    if (!js::GetPropertyKeys(cx, obj, flags, &props))
        return fail(cx, rs);

    for (size_t i = 0; i < props.length(); i++) {
        JSIDVariant id;
        if (!toJSIDVariant(cx, props[i], &id))
            return fail(cx, rs);

        ids->AppendElement(id);
    }

    return ok(rs);
}

bool
WrapperAnswer::RecvInstanceOf(const ObjectId &objId, const JSIID &iid, ReturnStatus *rs,
                              bool *instanceof)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();

    *instanceof = false;

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.instanceOf()", ReceiverObj(objId));

    nsID nsiid;
    ConvertID(iid, &nsiid);

    nsresult rv = xpc::HasInstance(cx, obj, &nsiid, instanceof);
    if (rv != NS_OK)
        return fail(cx, rs);

    return ok(rs);
}

bool
WrapperAnswer::RecvDOMInstanceOf(const ObjectId &objId, const int &prototypeID,
                                 const int &depth, ReturnStatus *rs, bool *instanceof)
{
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects())))
        return false;
    JSContext *cx = jsapi.cx();
    *instanceof = false;

    RootedObject obj(cx, findObjectById(cx, objId));
    if (!obj)
        return fail(cx, rs);

    LOG("%s.domInstanceOf()", ReceiverObj(objId));

    bool tmp;
    if (!mozilla::dom::InterfaceHasInstance(cx, prototypeID, depth, obj, &tmp))
        return fail(cx, rs);
    *instanceof = tmp;

    return ok(rs);
}

bool
WrapperAnswer::RecvDropObject(const ObjectId &objId)
{
    JSObject *obj = objects_.find(objId);
    if (obj) {
        objectIdMap(objId.hasXrayWaiver()).remove(obj);
        objects_.remove(objId);
    }
    return true;
}
