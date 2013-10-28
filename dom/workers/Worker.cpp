/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worker.h"

#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/EventHandlerBinding.h"
#include "nsJSUtils.h"

#include "jsapi.h"
#include "EventTarget.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

#include "WorkerInlines.h"

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

USING_WORKERS_NAMESPACE

using namespace mozilla::dom;
using mozilla::ErrorResult;

namespace {

class Worker
{
  static const DOMJSClass sClass;
  static const DOMIfaceAndProtoJSClass sProtoClass;
  static const JSPropertySpec sProperties[];
  static const JSFunctionSpec sFunctions[];

protected:
  enum {
    // The constructor function holds a WorkerPrivate* in its first reserved
    // slot.
    CONSTRUCTOR_SLOT_PARENT = 0
  };

public:
  static const JSClass*
  Class()
  {
    return sClass.ToJSClass();
  }

  static const JSClass*
  ProtoClass()
  {
    return sProtoClass.ToJSClass();
  }

  static const DOMClass*
  DOMClassStruct()
  {
    return &sClass.mClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto,
            bool aMainRuntime)
  {
    JS::Rooted<JSObject*> proto(aCx,
      js::InitClassWithReserved(aCx, aObj, aParentProto, ProtoClass(),
                                Construct, 0, sProperties, sFunctions,
                                nullptr, nullptr));
    if (!proto) {
      return nullptr;
    }

    js::SetReservedSlot(proto, DOM_PROTO_INSTANCE_CLASS_SLOT,
                        JS::PrivateValue(const_cast<DOMClass *>(DOMClassStruct())));

    if (!aMainRuntime) {
      WorkerPrivate* parent = GetWorkerPrivateFromContext(aCx);
      parent->AssertIsOnWorkerThread();

      JSObject* constructor = JS_GetConstructor(aCx, proto);
      if (!constructor)
        return nullptr;
      js::SetFunctionNativeReserved(constructor, CONSTRUCTOR_SLOT_PARENT,
                                    PRIVATE_TO_JSVAL(parent));
    }

    return proto;
  }

  static WorkerPrivate*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName);

  static JSObject*
  Create(JSContext* aCx, WorkerPrivate* aParentObj, const nsAString& aScriptURL,
         bool aIsChromeWorker, bool aIsSharedWorker,
         const nsAString& aSharedWorkerName);

protected:
  static bool
  ConstructInternal(JSContext* aCx, JS::CallArgs aArgs, bool aIsChromeWorker)
  {
    if (!aArgs.length()) {
      JS_ReportError(aCx, "Constructor requires at least one argument!");
      return false;
    }

    JS::RootedString scriptURLStr(aCx, JS_ValueToString(aCx, aArgs[0]));
    if (!scriptURLStr) {
      return false;
    }

    nsDependentJSString scriptURL;
    if (!scriptURL.init(aCx, scriptURLStr)) {
      return false;
    }

    JS::Rooted<JS::Value> priv(aCx,
      js::GetFunctionNativeReserved(&aArgs.callee(), CONSTRUCTOR_SLOT_PARENT));

    WorkerPrivate* parent;
    if (priv.isUndefined()) {
      parent = nullptr;
    } else {
      parent = static_cast<WorkerPrivate*>(priv.get().toPrivate());
      parent->AssertIsOnWorkerThread();
    }

    JS::Rooted<JSObject*> obj(aCx,
      Create(aCx, parent, scriptURL, aIsChromeWorker, false, EmptyString()));
    if (!obj) {
      return false;
    }

    aArgs.rval().setObject(*obj);
    return true;
  }

private:
  // No instance of this class should ever be created so these are explicitly
  // left without an implementation to prevent linking in case someone tries to
  // make one.
  Worker();
  ~Worker();

  static bool
  IsWorker(JS::Handle<JS::Value> v)
  {
    return v.isObject() && ClassIsWorker(JS_GetClass(&v.toObject()));
  }

  static bool
  GetEventListener(JSContext* aCx, const JS::CallArgs aArgs,
                   const nsAString &aNameStr)
  {
    WorkerPrivate* worker =
      GetInstancePrivate(aCx, &aArgs.thisv().toObject(),
                         NS_ConvertUTF16toUTF8(aNameStr).get());
    MOZ_ASSERT(worker);

    ErrorResult rv;
    nsRefPtr<EventHandlerNonNull> handler =
      worker->GetEventListener(Substring(aNameStr, 2), rv);

    if (rv.Failed()) {
      JS_ReportError(aCx, "Failed to get listener!");
      return false;
    }

    if (!handler) {
      aArgs.rval().setNull();
    } else {
      aArgs.rval().setObject(*handler->Callable());
    }
    return true;
  }

  static bool
  GetOnerrorImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    return GetEventListener(aCx, aArgs, NS_LITERAL_STRING("onerror"));
  }

  static bool
  GetOnerror(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    return JS::CallNonGenericMethod<IsWorker, GetOnerrorImpl>(aCx, args);
  }

  static bool
  GetOnmessageImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    return GetEventListener(aCx, aArgs, NS_LITERAL_STRING("onmessage"));
  }

  static bool
  GetOnmessage(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    return JS::CallNonGenericMethod<IsWorker, GetOnmessageImpl>(aCx, args);
  }

  static bool
  SetEventListener(JSContext* aCx, JS::CallArgs aArgs,
                   const nsAString& aNameStr)
  {
    WorkerPrivate* worker =
      GetInstancePrivate(aCx, &aArgs.thisv().toObject(),
                         NS_ConvertUTF16toUTF8(aNameStr).get());
    MOZ_ASSERT(worker);

    JS::Rooted<JSObject*> listener(aCx);
    if (!JS_ValueToObject(aCx, aArgs.get(0), &listener)) {
      return false;
    }

    nsRefPtr<EventHandlerNonNull> handler;
    if (listener && JS_ObjectIsCallable(aCx, listener)) {
      handler = new EventHandlerNonNull(listener);
    } else {
      handler = nullptr;
    }
    ErrorResult rv;
    worker->SetEventListener(Substring(aNameStr, 2), handler, rv);

    if (rv.Failed()) {
      JS_ReportError(aCx, "Failed to set listener!");
      return false;
    }

    aArgs.rval().setUndefined();
    return true;
  }

  static bool
  SetOnerrorImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    return SetEventListener(aCx, aArgs, NS_LITERAL_STRING("onerror"));
  }

  static bool
  SetOnerror(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    return JS::CallNonGenericMethod<IsWorker, SetOnerrorImpl>(aCx, args);
  }

  static bool
  SetOnmessageImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    return SetEventListener(aCx, aArgs, NS_LITERAL_STRING("onmessage"));
  }

  static bool
  SetOnmessage(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    return JS::CallNonGenericMethod<IsWorker, SetOnmessageImpl>(aCx, args);
  }

  static bool
  Construct(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    return ConstructInternal(aCx, args, false);
  }

  static void
  Finalize(JSFreeOp* aFop, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj);
    if (worker) {
      worker->_finalize(aFop);
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj);
    if (worker) {
      worker->_trace(aTrc);
    }
  }

  static bool
  Terminate(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    const char* name = sFunctions[0].name;
    WorkerPrivate* worker = GetInstancePrivate(aCx, obj, name);
    if (!worker) {
      return !JS_IsExceptionPending(aCx);
    }

    if (!worker->Terminate(aCx)) {
      return false;
    }

    JS_RVAL(aCx, aVp).setUndefined();
    return true;
  }

  static bool
  PostMessage(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    const char* name = sFunctions[1].name;
    WorkerPrivate* worker = GetInstancePrivate(aCx, obj, name);
    if (!worker) {
      return !JS_IsExceptionPending(aCx);
    }

    JS::Rooted<JS::Value> message(aCx);
    JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v/v",
                             message.address(), transferable.address())) {
      return false;
    }

    if (!worker->PostMessage(aCx, message, transferable)) {
      return false;
    }

    JS_RVAL(aCx, aVp).setUndefined();
    return true;
  }
};

const DOMJSClass Worker::sClass = {
  {
    "Worker",
    JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(3) |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
    nullptr, nullptr, nullptr, nullptr, Trace
  },
  {
    INTERFACE_CHAIN_1(prototypes::id::EventTarget_workers),
    false,
    &sWorkerNativePropertyHooks
  }
};

const DOMIfaceAndProtoJSClass Worker::sProtoClass = {
  {
    // XXXbz we use "Worker" here to match sClass so that we can
    // js::InitClassWithReserved this JSClass and then call
    // JS_NewObject with our sClass and have it find the right
    // prototype.
    "Worker",
    JSCLASS_IS_DOMIFACEANDPROTOJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,               /* finalize */
    nullptr,               /* checkAccess */
    nullptr,               /* call */
    nullptr,               /* hasInstance */
    nullptr,               /* construct */
    nullptr,               /* trace */
    JSCLASS_NO_INTERNAL_MEMBERS
  },
  eInterfacePrototype,
  &sWorkerNativePropertyHooks,
  "[object Worker]",
  prototypes::id::_ID_Count,
  0
};

const JSPropertySpec Worker::sProperties[] = {
  JS_PSGS("onerror", GetOnerror, SetOnerror, JSPROP_ENUMERATE),
  JS_PSGS("onmessage", GetOnmessage, SetOnmessage, JSPROP_ENUMERATE),
  JS_PS_END
};

const JSFunctionSpec Worker::sFunctions[] = {
  JS_FN("terminate", Terminate, 0, FUNCTION_FLAGS),
  JS_FN("postMessage", PostMessage, 1, FUNCTION_FLAGS),
  JS_FS_END
};

class ChromeWorker : public Worker
{
  static const DOMJSClass sClass;
  static const DOMIfaceAndProtoJSClass sProtoClass;

public:
  static const JSClass*
  Class()
  {
    return sClass.ToJSClass();
  }

  static const JSClass*
  ProtoClass()
  {
    return sProtoClass.ToJSClass();
  }

  static const DOMClass*
  DOMClassStruct()
  {
    return &sClass.mClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto,
            bool aMainRuntime)
  {
    JS::Rooted<JSObject*> proto(aCx,
      js::InitClassWithReserved(aCx, aObj, aParentProto, ProtoClass(),
                                Construct, 0, nullptr, nullptr, nullptr,
                                nullptr));
    if (!proto) {
      return nullptr;
    }

    js::SetReservedSlot(proto, DOM_PROTO_INSTANCE_CLASS_SLOT,
                        JS::PrivateValue(const_cast<DOMClass *>(DOMClassStruct())));

    if (!aMainRuntime) {
      WorkerPrivate* parent = GetWorkerPrivateFromContext(aCx);
      parent->AssertIsOnWorkerThread();

      JSObject* constructor = JS_GetConstructor(aCx, proto);
      if (!constructor)
        return nullptr;
      js::SetFunctionNativeReserved(constructor, CONSTRUCTOR_SLOT_PARENT,
                                    PRIVATE_TO_JSVAL(parent));
    }

    return proto;
  }

private:
  // No instance of this class should ever be created so these are explicitly
  // left without an implementation to prevent linking in case someone tries to
  // make one.
  ChromeWorker();
  ~ChromeWorker();

  static WorkerPrivate*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    if (aObj) {
      const JSClass* classPtr = JS_GetClass(aObj);
      if (classPtr == Class()) {
        return UnwrapDOMObject<WorkerPrivate>(aObj);
      }
    }

    return Worker::GetInstancePrivate(aCx, aObj, aFunctionName);
  }

  static bool
  Construct(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
    return ConstructInternal(aCx, args, true);
  }

  static void
  Finalize(JSFreeOp* aFop, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj);
    if (worker) {
      worker->_finalize(aFop);
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    WorkerPrivate* worker = UnwrapDOMObject<WorkerPrivate>(aObj);
    if (worker) {
      worker->_trace(aTrc);
    }
  }
};

const DOMJSClass ChromeWorker::sClass = {
  { "ChromeWorker",
    JSCLASS_IS_DOMJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(3) |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
    nullptr, nullptr, nullptr, nullptr, Trace,
  },
  {
    INTERFACE_CHAIN_1(prototypes::id::EventTarget_workers),
    false,
    &sWorkerNativePropertyHooks
  }
};

const DOMIfaceAndProtoJSClass ChromeWorker::sProtoClass = {
  {
    // XXXbz we use "ChromeWorker" here to match sClass so that we can
    // js::InitClassWithReserved this JSClass and then call
    // JS_NewObject with our sClass and have it find the right
    // prototype.
    "ChromeWorker",
    JSCLASS_IS_DOMIFACEANDPROTOJSCLASS | JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,               /* finalize */
    nullptr,               /* checkAccess */
    nullptr,               /* call */
    nullptr,               /* hasInstance */
    nullptr,               /* construct */
    nullptr,               /* trace */
    JSCLASS_NO_INTERNAL_MEMBERS
  },
  eInterfacePrototype,
  &sWorkerNativePropertyHooks,
  "[object ChromeWorker]",
  prototypes::id::_ID_Count,
  0
};

WorkerPrivate*
Worker::GetInstancePrivate(JSContext* aCx, JSObject* aObj,
                           const char* aFunctionName)
{
  const JSClass* classPtr = JS_GetClass(aObj);
  if (ClassIsWorker(classPtr)) {
    return UnwrapDOMObject<WorkerPrivate>(aObj);
  }

  JS_ReportErrorNumber(aCx, js_GetErrorMessage, nullptr,
                       JSMSG_INCOMPATIBLE_PROTO, Class()->name,
                       aFunctionName, classPtr->name);
  return nullptr;
}

JSObject*
Worker::Create(JSContext* aCx, WorkerPrivate* aParent,
               const nsAString& aScriptURL, bool aIsChromeWorker,
               bool aIsSharedWorker, const nsAString& aSharedWorkerName)
{
  MOZ_ASSERT_IF(aIsSharedWorker, !aSharedWorkerName.IsVoid());
  MOZ_ASSERT_IF(!aIsSharedWorker, aSharedWorkerName.IsEmpty());

  RuntimeService* runtimeService;
  if (aParent) {
    runtimeService = RuntimeService::GetService();
    NS_ASSERTION(runtimeService, "Null runtime service!");
  }
  else {
    runtimeService = RuntimeService::GetOrCreateService();
    if (!runtimeService) {
      JS_ReportError(aCx, "Failed to create runtime service!");
      return nullptr;
    }
  }

  const JSClass* classPtr = aIsChromeWorker ? ChromeWorker::Class() : Class();

  JS::Rooted<JSObject*> obj(aCx,
    JS_NewObject(aCx, const_cast<JSClass*>(classPtr), nullptr, nullptr));
  if (!obj) {
    return nullptr;
  }

  // Ensure that the DOM_OBJECT_SLOT always has a PrivateValue set, as this will
  // be accessed in the Trace() method if WorkerPrivate::Create() triggers a GC.
  js::SetReservedSlot(obj, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));

  nsRefPtr<WorkerPrivate> worker =
    WorkerPrivate::Create(aCx, obj, aParent, aScriptURL, aIsChromeWorker,
                          aIsSharedWorker, aSharedWorkerName);
  if (!worker) {
    // It'd be better if we could avoid allocating the JSObject until after we
    // make sure we have a WorkerPrivate, but failing that we should at least
    // make sure that the DOM_OBJECT_SLOT always has a PrivateValue.
    return nullptr;
  }

  // Worker now owned by the JS object.
  NS_ADDREF(worker.get());
  js::SetReservedSlot(obj, DOM_OBJECT_SLOT, JS::PrivateValue(worker));

  if (!runtimeService->RegisterWorker(aCx, worker)) {
    return nullptr;
  }

  // Worker now also owned by its thread.
  NS_ADDREF(worker.get());

  return obj;
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace worker {

JSObject*
InitClass(JSContext* aCx, JSObject* aGlobal, JSObject* aProto,
          bool aMainRuntime)
{
  return Worker::InitClass(aCx, aGlobal, aProto, aMainRuntime);
}

} // namespace worker

WorkerCrossThreadDispatcher*
GetWorkerCrossThreadDispatcher(JSContext* aCx, jsval aWorker)
{
  if (JSVAL_IS_PRIMITIVE(aWorker)) {
    return nullptr;
  }

  WorkerPrivate* w =
      Worker::GetInstancePrivate(aCx, JSVAL_TO_OBJECT(aWorker),
                                 "GetWorkerCrossThreadDispatcher");
  if (!w) {
    return nullptr;
  }
  return w->GetCrossThreadDispatcher();
}


namespace chromeworker {

bool
InitClass(JSContext* aCx, JSObject* aGlobal, JSObject* aProto,
          bool aMainRuntime)
{
  return !!ChromeWorker::InitClass(aCx, aGlobal, aProto, aMainRuntime);
}

} // namespace chromeworker

bool
ClassIsWorker(const JSClass* aClass)
{
  return Worker::Class() == aClass || ChromeWorker::Class() == aClass;
}

bool
GetterOnlyJSNative(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, nullptr, JSMSG_GETTER_ONLY);
    return false;
}

END_WORKERS_NAMESPACE
