/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=4 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WrapperAnswer.h"
#include "JavaScriptLogging.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "xpcprivate.h"
#include "js/Class.h"
#include "js/RegExp.h"
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
using mozilla::dom::AutoEntryScript;
using mozilla::dom::AutoJSAPI;

using xpc::IsInAutomation;

static void MaybeForceDebugGC() {
  static bool sEnvVarInitialized = false;
  static bool sDebugGCs = false;

  if (!sEnvVarInitialized) {
    sDebugGCs = !!PR_GetEnv("MOZ_DEBUG_DEAD_CPOWS");
  }

  if (sDebugGCs) {
    JSContext* cx = XPCJSContext::Get()->Context();
    PrepareForFullGC(cx);
    NonIncrementalGC(cx, GC_NORMAL, GCReason::COMPONENT_UTILS);
  }
}

bool WrapperAnswer::fail(AutoJSAPI& jsapi, ReturnStatus* rs) {
  // By default, we set |undefined| unless we can get a more meaningful
  // exception.
  *rs = ReturnStatus(ReturnException(JSVariant(UndefinedVariant())));

  // Note we always return true from this function, since this propagates
  // to the IPC code, and we don't want a JS failure to cause the death
  // of the child process.

  JSContext* cx = jsapi.cx();
  RootedValue exn(cx);
  if (!jsapi.HasException()) {
    return true;
  }

  if (!jsapi.StealException(&exn)) {
    return true;
  }

  // If this fails, we still don't want to exit. Just return an invalid
  // exception.
  (void)toVariant(cx, exn, &rs->get_ReturnException().exn());
  return true;
}

bool WrapperAnswer::ok(ReturnStatus* rs) {
  *rs = ReturnStatus(ReturnSuccess());
  return true;
}

bool WrapperAnswer::ok(ReturnStatus* rs, const JS::ObjectOpResult& result) {
  *rs = result ? ReturnStatus(ReturnSuccess())
               : ReturnStatus(ReturnObjectOpResult(result.failureCode()));
  return true;
}

bool WrapperAnswer::deadCPOW(AutoJSAPI& jsapi, ReturnStatus* rs) {
  JSContext* cx = jsapi.cx();
  JS_ClearPendingException(cx);
  *rs = ReturnStatus(ReturnDeadCPOW());
  return true;
}

bool WrapperAnswer::RecvPreventExtensions(const ObjectId& objId,
                                          ReturnStatus* rs) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  ObjectOpResult success;
  if (!JS_PreventExtensions(cx, obj, success)) {
    return fail(jsapi, rs);
  }

  LOG("%s.preventExtensions()", ReceiverObj(objId));
  return ok(rs, success);
}

static void EmptyDesc(PPropertyDescriptor* desc) {
  desc->obj() = LocalObject(0);
  desc->attrs() = 0;
  desc->value() = UndefinedVariant();
  desc->getter() = 0;
  desc->setter() = 0;
}

bool WrapperAnswer::RecvGetOwnPropertyDescriptor(const ObjectId& objId,
                                                 const JSIDVariant& idVar,
                                                 ReturnStatus* rs,
                                                 PPropertyDescriptor* out) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();
  EmptyDesc(out);

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.getOwnPropertyDescriptor(%s)", ReceiverObj(objId), Identifier(idVar));

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(jsapi, rs);
  }

  Rooted<PropertyDescriptor> desc(cx);
  if (!JS_GetOwnPropertyDescriptorById(cx, obj, id, &desc)) {
    return fail(jsapi, rs);
  }

  if (!fromDescriptor(cx, desc, out)) {
    return fail(jsapi, rs);
  }

  return ok(rs);
}

bool WrapperAnswer::RecvDefineProperty(const ObjectId& objId,
                                       const JSIDVariant& idVar,
                                       const PPropertyDescriptor& descriptor,
                                       ReturnStatus* rs) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("define %s[%s]", ReceiverObj(objId), Identifier(idVar));

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(jsapi, rs);
  }

  Rooted<PropertyDescriptor> desc(cx);
  if (!toDescriptor(cx, descriptor, &desc)) {
    return fail(jsapi, rs);
  }

  ObjectOpResult success;
  if (!JS_DefinePropertyById(cx, obj, id, desc, success)) {
    return fail(jsapi, rs);
  }
  return ok(rs, success);
}

bool WrapperAnswer::RecvDelete(const ObjectId& objId, const JSIDVariant& idVar,
                               ReturnStatus* rs) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("delete %s[%s]", ReceiverObj(objId), Identifier(idVar));

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(jsapi, rs);
  }

  ObjectOpResult success;
  if (!JS_DeletePropertyById(cx, obj, id, success)) {
    return fail(jsapi, rs);
  }
  return ok(rs, success);
}

bool WrapperAnswer::RecvHas(const ObjectId& objId, const JSIDVariant& idVar,
                            ReturnStatus* rs, bool* foundp) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();
  *foundp = false;

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.has(%s)", ReceiverObj(objId), Identifier(idVar));

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(jsapi, rs);
  }

  if (!JS_HasPropertyById(cx, obj, id, foundp)) {
    return fail(jsapi, rs);
  }
  return ok(rs);
}

bool WrapperAnswer::RecvHasOwn(const ObjectId& objId, const JSIDVariant& idVar,
                               ReturnStatus* rs, bool* foundp) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();
  *foundp = false;

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.hasOwn(%s)", ReceiverObj(objId), Identifier(idVar));

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(jsapi, rs);
  }

  if (!JS_HasOwnPropertyById(cx, obj, id, foundp)) {
    return fail(jsapi, rs);
  }
  return ok(rs);
}

bool WrapperAnswer::RecvGet(const ObjectId& objId, const JSVariant& receiverVar,
                            const JSIDVariant& idVar, ReturnStatus* rs,
                            JSVariant* result) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  // We may run scripted getters.
  AutoEntryScript aes(scopeForTargetObjects(),
                      "Cross-Process Object Wrapper 'get'");
  JSContext* cx = aes.cx();

  // The outparam will be written to the buffer, so it must be set even if
  // the parent won't read it.
  *result = UndefinedVariant();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(aes, rs);
  }

  RootedValue receiver(cx);
  if (!fromVariant(cx, receiverVar, &receiver)) {
    return fail(aes, rs);
  }

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(aes, rs);
  }

  JS::RootedValue val(cx);
  if (!JS_ForwardGetPropertyTo(cx, obj, id, receiver, &val)) {
    return fail(aes, rs);
  }

  if (!toVariant(cx, val, result)) {
    return fail(aes, rs);
  }

  LOG("get %s.%s = %s", ReceiverObj(objId), Identifier(idVar),
      OutVariant(*result));

  return ok(rs);
}

bool WrapperAnswer::RecvSet(const ObjectId& objId, const JSIDVariant& idVar,
                            const JSVariant& value,
                            const JSVariant& receiverVar, ReturnStatus* rs) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  // We may run scripted setters.
  AutoEntryScript aes(scopeForTargetObjects(),
                      "Cross-Process Object Wrapper 'set'");
  JSContext* cx = aes.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(aes, rs);
  }

  LOG("set %s[%s] = %s", ReceiverObj(objId), Identifier(idVar),
      InVariant(value));

  RootedId id(cx);
  if (!fromJSIDVariant(cx, idVar, &id)) {
    return fail(aes, rs);
  }

  RootedValue val(cx);
  if (!fromVariant(cx, value, &val)) {
    return fail(aes, rs);
  }

  RootedValue receiver(cx);
  if (!fromVariant(cx, receiverVar, &receiver)) {
    return fail(aes, rs);
  }

  ObjectOpResult result;
  if (!JS_ForwardSetPropertyTo(cx, obj, id, val, receiver, result)) {
    return fail(aes, rs);
  }

  return ok(rs, result);
}

bool WrapperAnswer::RecvIsExtensible(const ObjectId& objId, ReturnStatus* rs,
                                     bool* result) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();
  *result = false;

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.isExtensible()", ReceiverObj(objId));

  bool extensible;
  if (!JS_IsExtensible(cx, obj, &extensible)) {
    return fail(jsapi, rs);
  }

  *result = !!extensible;
  return ok(rs);
}

bool WrapperAnswer::RecvCallOrConstruct(const ObjectId& objId,
                                        InfallibleTArray<JSParam>&& argv,
                                        const bool& construct, ReturnStatus* rs,
                                        JSVariant* result,
                                        nsTArray<JSParam>* outparams) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoEntryScript aes(scopeForTargetObjects(),
                      "Cross-Process Object Wrapper call/construct");
  JSContext* cx = aes.cx();

  // The outparam will be written to the buffer, so it must be set even if
  // the parent won't read it.
  *result = UndefinedVariant();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(aes, rs);
  }

  MOZ_ASSERT(argv.Length() >= 2);

  RootedValue objv(cx);
  if (!fromVariant(cx, argv[0], &objv)) {
    return fail(aes, rs);
  }

  *result = JSVariant(UndefinedVariant());

  RootedValueVector vals(cx);
  RootedValueVector outobjects(cx);
  for (size_t i = 0; i < argv.Length(); i++) {
    if (argv[i].type() == JSParam::Tvoid_t) {
      // This is an outparam.
      RootedObject obj(cx, xpc::NewOutObject(cx));
      if (!obj) {
        return fail(aes, rs);
      }
      if (!outobjects.append(ObjectValue(*obj))) {
        return fail(aes, rs);
      }
      if (!vals.append(ObjectValue(*obj))) {
        return fail(aes, rs);
      }
    } else {
      RootedValue v(cx);
      if (!fromVariant(cx, argv[i].get_JSVariant(), &v)) {
        return fail(aes, rs);
      }
      if (!vals.append(v)) {
        return fail(aes, rs);
      }
    }
  }

  RootedValue rval(cx);
  {
    HandleValueArray args =
        HandleValueArray::subarray(vals, 2, vals.length() - 2);
    if (construct) {
      RootedObject obj(cx);
      if (!JS::Construct(cx, vals[0], args, &obj)) {
        return fail(aes, rs);
      }
      rval.setObject(*obj);
    } else {
      if (!JS::Call(cx, vals[1], vals[0], args, &rval)) return fail(aes, rs);
    }
  }

  if (!toVariant(cx, rval, result)) {
    return fail(aes, rs);
  }

  // Prefill everything with a dummy jsval.
  for (size_t i = 0; i < outobjects.length(); i++) {
    outparams->AppendElement(JSParam(void_t()));
  }

  // Go through each argument that was an outparam, retrieve the "value"
  // field, and add it to a temporary list. We need to do this separately
  // because the outparams vector is not rooted.
  vals.clear();
  for (size_t i = 0; i < outobjects.length(); i++) {
    RootedObject obj(cx, &outobjects[i].toObject());

    RootedValue v(cx);
    bool found;
    if (JS_HasProperty(cx, obj, "value", &found)) {
      if (!JS_GetProperty(cx, obj, "value", &v)) {
        return fail(aes, rs);
      }
    } else {
      v = UndefinedValue();
    }
    if (!vals.append(v)) {
      return fail(aes, rs);
    }
  }

  // Copy the outparams. If any outparam is already set to a void_t, we
  // treat this as the outparam never having been set.
  for (size_t i = 0; i < vals.length(); i++) {
    JSVariant variant;
    if (!toVariant(cx, vals[i], &variant)) {
      return fail(aes, rs);
    }
    outparams->ReplaceElementAt(i, JSParam(variant));
  }

  LOG("%s.call(%s) = %s", ReceiverObj(objId), argv, OutVariant(*result));

  return ok(rs);
}

bool WrapperAnswer::RecvHasInstance(const ObjectId& objId,
                                    const JSVariant& vVar, ReturnStatus* rs,
                                    bool* bp) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.hasInstance(%s)", ReceiverObj(objId), InVariant(vVar));

  RootedValue val(cx);
  if (!fromVariant(cx, vVar, &val)) {
    return fail(jsapi, rs);
  }

  if (!JS_HasInstance(cx, obj, val, bp)) {
    return fail(jsapi, rs);
  }

  return ok(rs);
}

bool WrapperAnswer::RecvGetBuiltinClass(const ObjectId& objId, ReturnStatus* rs,
                                        uint32_t* classValue) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  *classValue = uint32_t(js::ESClass::Other);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.getBuiltinClass()", ReceiverObj(objId));

  js::ESClass cls;
  if (!js::GetBuiltinClass(cx, obj, &cls)) {
    return fail(jsapi, rs);
  }

  *classValue = uint32_t(cls);
  return ok(rs);
}

bool WrapperAnswer::RecvIsArray(const ObjectId& objId, ReturnStatus* rs,
                                uint32_t* ans) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  *ans = uint32_t(IsArrayAnswer::NotArray);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.isArray()", ReceiverObj(objId));

  IsArrayAnswer answer;
  if (!JS::IsArray(cx, obj, &answer)) {
    return fail(jsapi, rs);
  }

  *ans = uint32_t(answer);
  return ok(rs);
}

bool WrapperAnswer::RecvClassName(const ObjectId& objId, nsCString* name) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    // This is very unfortunate, but we have no choice.
    *name = "<dead CPOW>";
    return true;
  }

  LOG("%s.className()", ReceiverObj(objId));

  *name = js::ObjectClassName(cx, obj);
  return true;
}

bool WrapperAnswer::RecvGetPrototype(const ObjectId& objId, ReturnStatus* rs,
                                     ObjectOrNullVariant* result) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  *result = NullVariant();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  JS::RootedObject proto(cx);
  if (!JS_GetPrototype(cx, obj, &proto)) {
    return fail(jsapi, rs);
  }

  if (!toObjectOrNullVariant(cx, proto, result)) {
    return fail(jsapi, rs);
  }

  LOG("getPrototype(%s)", ReceiverObj(objId));

  return ok(rs);
}

bool WrapperAnswer::RecvGetPrototypeIfOrdinary(const ObjectId& objId,
                                               ReturnStatus* rs,
                                               bool* isOrdinary,
                                               ObjectOrNullVariant* result) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  *result = NullVariant();
  *isOrdinary = false;

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  JS::RootedObject proto(cx);
  if (!JS_GetPrototypeIfOrdinary(cx, obj, isOrdinary, &proto)) {
    return fail(jsapi, rs);
  }

  if (!toObjectOrNullVariant(cx, proto, result)) {
    return fail(jsapi, rs);
  }

  LOG("getPrototypeIfOrdinary(%s)", ReceiverObj(objId));

  return ok(rs);
}

bool WrapperAnswer::RecvRegExpToShared(const ObjectId& objId, ReturnStatus* rs,
                                       nsString* source, uint32_t* flags) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  RootedString sourceJSStr(cx, JS::GetRegExpSource(cx, obj));
  if (!sourceJSStr) {
    return fail(jsapi, rs);
  }
  nsAutoJSString sourceStr;
  if (!sourceStr.init(cx, sourceJSStr)) {
    return fail(jsapi, rs);
  }
  source->Assign(sourceStr);

  *flags = JS::GetRegExpFlags(cx, obj).value();

  return ok(rs);
}

bool WrapperAnswer::RecvGetPropertyKeys(const ObjectId& objId,
                                        const uint32_t& flags, ReturnStatus* rs,
                                        nsTArray<JSIDVariant>* ids) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.getPropertyKeys()", ReceiverObj(objId));

  RootedIdVector props(cx);
  if (!js::GetPropertyKeys(cx, obj, flags, &props)) {
    return fail(jsapi, rs);
  }

  for (size_t i = 0; i < props.length(); i++) {
    JSIDVariant id;
    if (!toJSIDVariant(cx, props[i], &id)) {
      return fail(jsapi, rs);
    }

    ids->AppendElement(id);
  }

  return ok(rs);
}

bool WrapperAnswer::RecvInstanceOf(const ObjectId& objId, const JSIID& iid,
                                   ReturnStatus* rs, bool* instanceof) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  * instanceof = false;

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.instanceOf()", ReceiverObj(objId));

  nsID nsiid;
  ConvertID(iid, &nsiid);

  nsresult rv = xpc::HasInstance(cx, obj, &nsiid, instanceof);
  if (rv != NS_OK) {
    return fail(jsapi, rs);
  }

  return ok(rs);
}

bool WrapperAnswer::RecvDOMInstanceOf(const ObjectId& objId,
                                      const int& prototypeID, const int& depth,
                                      ReturnStatus* rs, bool* instanceof) {
  if (!IsInAutomation()) {
    return false;
  }

  MaybeForceDebugGC();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(scopeForTargetObjects()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();
  * instanceof = false;

  RootedObject obj(cx, findObjectById(cx, objId));
  if (!obj) {
    return deadCPOW(jsapi, rs);
  }

  LOG("%s.domInstanceOf()", ReceiverObj(objId));

  bool tmp;
  if (!mozilla::dom::InterfaceHasInstance(cx, prototypeID, depth, obj, &tmp)) {
    return fail(jsapi, rs);
  }
  * instanceof = tmp;

  return ok(rs);
}

bool WrapperAnswer::RecvDropObject(const ObjectId& objId) {
  JSObject* obj = objects_.findPreserveColor(objId);
  if (obj) {
    objectIdMap(objId.hasXrayWaiver()).remove(obj);
    objects_.remove(objId);
  }
  return true;
}
