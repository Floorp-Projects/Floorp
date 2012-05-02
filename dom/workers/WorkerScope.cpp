/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "WorkerScope.h"

#include "jsapi.h"
#include "jsdbgapi.h"
#include "mozilla/Util.h"
#include "mozilla/dom/bindings/DOMJSClass.h"
#include "mozilla/dom/bindings/EventTargetBinding.h"
#include "mozilla/dom/bindings/Utils.h"
#include "mozilla/dom/bindings/XMLHttpRequestBinding.h"
#include "mozilla/dom/bindings/XMLHttpRequestUploadBinding.h"
#include "nsTraceRefcnt.h"
#include "xpcpublic.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#include "ChromeWorkerScope.h"
#include "Events.h"
#include "EventListenerManager.h"
#include "EventTarget.h"
#include "Exceptions.h"
#include "File.h"
#include "FileReaderSync.h"
#include "Location.h"
#include "ImageData.h"
#include "Navigator.h"
#include "Principal.h"
#include "ScriptLoader.h"
#include "Worker.h"
#include "WorkerPrivate.h"
#include "XMLHttpRequest.h"

#include "WorkerInlines.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

using namespace mozilla;
using namespace mozilla::dom::bindings;
USING_WORKERS_NAMESPACE

namespace {

class WorkerGlobalScope : public EventTarget
{
  static JSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

  enum
  {
    SLOT_wrappedScope = 0,
    SLOT_wrappedFunction
  };

  enum
  {
    SLOT_location = 0,
    SLOT_navigator,

    SLOT_COUNT
  };

  // Must be traced!
  jsval mSlots[SLOT_COUNT];

  enum
  {
    STRING_onerror = 0,
    STRING_onclose,

    STRING_COUNT
  };

  static const char* const sEventStrings[STRING_COUNT];

protected:
  WorkerPrivate* mWorker;

public:
  static JSClass*
  Class()
  {
    return &sClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto)
  {
    return JS_InitClass(aCx, aObj, aParentProto, Class(), Construct, 0,
                        sProperties, sFunctions, NULL, NULL);
  }

protected:
  WorkerGlobalScope(JSContext* aCx, WorkerPrivate* aWorker)
  : EventTarget(aCx), mWorker(aWorker)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::WorkerGlobalScope);
    for (int32 i = 0; i < SLOT_COUNT; i++) {
      mSlots[i] = JSVAL_VOID;
    }
  }

  ~WorkerGlobalScope()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::WorkerGlobalScope);
  }

  virtual void
  _Trace(JSTracer* aTrc) MOZ_OVERRIDE
  {
    for (int32 i = 0; i < SLOT_COUNT; i++) {
      JS_CALL_VALUE_TRACER(aTrc, mSlots[i], "WorkerGlobalScope instance slot");
    }
    mWorker->TraceInternal(aTrc);
    EventTarget::_Trace(aTrc);
  }

  virtual void
  _Finalize(JSFreeOp* aFop) MOZ_OVERRIDE
  {
    EventTarget::_Finalize(aFop);
  }

private:
  static JSBool
  _GetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];
    WorkerGlobalScope* scope = GetInstancePrivate(aCx, aObj, name);
    if (!scope) {
      return false;
    }

    nsresult rv = NS_OK;

    JSObject* listener =
      scope->GetEventListener(NS_ConvertASCIItoUTF16(name + 2), rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to get event listener!");
      return false;
    }

    *aVp = listener ? OBJECT_TO_JSVAL(listener) : JSVAL_NULL;
    return true;
  }

  static JSBool
  _SetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, JSBool aStrict,
                   jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];
    WorkerGlobalScope* scope = GetInstancePrivate(aCx, aObj, name);
    if (!scope) {
      return false;
    }

    if (JSVAL_IS_PRIMITIVE(*aVp)) {
      JS_ReportError(aCx, "Not an event listener!");
      return false;
    }

    nsresult rv = NS_OK;
    scope->SetEventListener(NS_ConvertASCIItoUTF16(name + 2),
                            JSVAL_TO_OBJECT(*aVp), rv);
    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to set event listener!");
      return false;
    }

    return true;
  }

  static WorkerGlobalScope*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName);

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                         sClass.name);
    return false;
  }

  static JSBool
  GetSelf(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    if (!GetInstancePrivate(aCx, aObj, "self")) {
      return false;
    }

    *aVp = OBJECT_TO_JSVAL(aObj);
    return true;
  }

  static JSBool
  GetLocation(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    WorkerGlobalScope* scope =
      GetInstancePrivate(aCx, aObj, sProperties[SLOT_location].name);
    if (!scope) {
      return false;
    }

    if (JSVAL_IS_VOID(scope->mSlots[SLOT_location])) {
      JSString* href, *protocol, *host, *hostname;
      JSString* port, *pathname, *search, *hash;

      WorkerPrivate::LocationInfo& info = scope->mWorker->GetLocationInfo();

#define COPY_STRING(_jsstr, _cstr)                                             \
  if (info. _cstr .IsEmpty()) {                                                \
    _jsstr = NULL;                                                             \
  }                                                                            \
  else {                                                                       \
    if (!(_jsstr = JS_NewStringCopyN(aCx, info. _cstr .get(),                  \
                                     info. _cstr .Length()))) {                \
      return false;                                                            \
    }                                                                          \
    info. _cstr .Truncate();                                                   \
  }

      COPY_STRING(href, mHref);
      COPY_STRING(protocol, mProtocol);
      COPY_STRING(host, mHost);
      COPY_STRING(hostname, mHostname);
      COPY_STRING(port, mPort);
      COPY_STRING(pathname, mPathname);
      COPY_STRING(search, mSearch);
      COPY_STRING(hash, mHash);

#undef COPY_STRING

      JSObject* location = location::Create(aCx, href, protocol, host, hostname,
                                            port, pathname, search, hash);
      if (!location) {
        return false;
      }

      scope->mSlots[SLOT_location] = OBJECT_TO_JSVAL(location);
    }

    *aVp = scope->mSlots[SLOT_location];
    return true;
  }

  static JSBool
  UnwrapErrorEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS_ASSERT(JSVAL_IS_OBJECT(JS_CALLEE(aCx, aVp)));
    JS_ASSERT(aArgc == 1);
    JS_ASSERT(JSVAL_IS_OBJECT(JS_ARGV(aCx, aVp)[0]));

    JSObject* wrapper = JSVAL_TO_OBJECT(JS_CALLEE(aCx, aVp));
    JS_ASSERT(JS_ObjectIsFunction(aCx, wrapper));

    jsval scope = js::GetFunctionNativeReserved(wrapper, SLOT_wrappedScope);
    jsval listener = js::GetFunctionNativeReserved(wrapper, SLOT_wrappedFunction);

    JS_ASSERT(JSVAL_IS_OBJECT(scope));

    JSObject* event = JSVAL_TO_OBJECT(JS_ARGV(aCx, aVp)[0]);

    jsval argv[3] = { JSVAL_VOID, JSVAL_VOID, JSVAL_VOID };
    if (!JS_GetProperty(aCx, event, "message", &argv[0]) ||
        !JS_GetProperty(aCx, event, "filename", &argv[1]) ||
        !JS_GetProperty(aCx, event, "lineno", &argv[2])) {
      return false;
    }

    jsval rval = JSVAL_VOID;
    if (!JS_CallFunctionValue(aCx, JSVAL_TO_OBJECT(scope), listener,
                              ArrayLength(argv), argv, &rval)) {
      JS_ReportPendingException(aCx);
      return false;
    }

    if (JSVAL_IS_BOOLEAN(rval) && JSVAL_TO_BOOLEAN(rval) &&
        !JS_CallFunctionName(aCx, event, "preventDefault", 0, NULL, &rval)) {
      return false;
    }

    return true;
  }

  static JSBool
  GetOnErrorListener(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    const char* name = sEventStrings[STRING_onerror];
    WorkerGlobalScope* scope = GetInstancePrivate(aCx, aObj, name);
    if (!scope) {
      return false;
    }

    nsresult rv = NS_OK;

    JSObject* adaptor =
      scope->GetEventListener(NS_ConvertASCIItoUTF16(name + 2), rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to get event listener!");
      return false;
    }

    if (!adaptor) {
      *aVp = JSVAL_NULL;
      return true;
    }

    *aVp = js::GetFunctionNativeReserved(adaptor, SLOT_wrappedFunction);

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(*aVp));

    return true;
  }

  static JSBool
  SetOnErrorListener(JSContext* aCx, JSObject* aObj, jsid aIdval,
                     JSBool aStrict, jsval* aVp)
  {
    const char* name = sEventStrings[STRING_onerror];
    WorkerGlobalScope* scope = GetInstancePrivate(aCx, aObj, name);
    if (!scope) {
      return false;
    }

    if (JSVAL_IS_PRIMITIVE(*aVp)) {
      JS_ReportError(aCx, "Not an event listener!");
      return false;
    }

    JSFunction* adaptor =
      js::NewFunctionWithReserved(aCx, UnwrapErrorEvent, 1, 0,
                                  JS_GetGlobalObject(aCx), "unwrap");
    if (!adaptor) {
      return false;
    }

    JSObject* listener = JS_GetFunctionObject(adaptor);
    if (!listener) {
      return false;
    }

    js::SetFunctionNativeReserved(listener, SLOT_wrappedScope,
                                  OBJECT_TO_JSVAL(aObj));
    js::SetFunctionNativeReserved(listener, SLOT_wrappedFunction, *aVp);

    nsresult rv = NS_OK;

    scope->SetEventListener(NS_ConvertASCIItoUTF16(name + 2), listener, rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to set event listener!");
      return false;
    }

    return true;
  }

  static JSBool
  GetNavigator(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    WorkerGlobalScope* scope =
      GetInstancePrivate(aCx, aObj, sProperties[SLOT_navigator].name);
    if (!scope) {
      return false;
    }

    if (JSVAL_IS_VOID(scope->mSlots[SLOT_navigator])) {
      JSObject* navigator = navigator::Create(aCx);
      if (!navigator) {
        return false;
      }

      scope->mSlots[SLOT_navigator] = OBJECT_TO_JSVAL(navigator);
    }

    *aVp = scope->mSlots[SLOT_navigator];
    return true;
  }

  static JSBool
  Close(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    WorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!scope) {
      return false;
    }

    return scope->mWorker->CloseInternal(aCx);
  }

  static JSBool
  ImportScripts(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    WorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, sFunctions[1].name);
    if (!scope) {
      return false;
    }

    if (aArgc && !scriptloader::Load(aCx, aArgc, JS_ARGV(aCx, aVp))) {
      return false;
    }

    return true;
  }

  static JSBool
  SetTimeout(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    WorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, sFunctions[2].name);
    if (!scope) {
      return false;
    }

    jsval dummy;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &dummy)) {
      return false;
    }

    return scope->mWorker->SetTimeout(aCx, aArgc, aVp, false);
  }

  static JSBool
  ClearTimeout(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    WorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, sFunctions[3].name);
    if (!scope) {
      return false;
    }

    uint32_t id;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "u", &id)) {
      return false;
    }

    return scope->mWorker->ClearTimeout(aCx, id);
  }

  static JSBool
  SetInterval(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    WorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, sFunctions[4].name);
    if (!scope) {
      return false;
    }

    jsval dummy;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &dummy)) {
      return false;
    }

    return scope->mWorker->SetTimeout(aCx, aArgc, aVp, true);
  }

  static JSBool
  ClearInterval(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    WorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, sFunctions[5].name);
    if (!scope) {
      return false;
    }

    uint32_t id;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "u", &id)) {
      return false;
    }

    return scope->mWorker->ClearTimeout(aCx, id);
  }

  static JSBool
  Dump(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    if (!GetInstancePrivate(aCx, obj, sFunctions[6].name)) {
      return false;
    }

    if (aArgc) {
      JSString* str = JS_ValueToString(aCx, JS_ARGV(aCx, aVp)[0]);
      if (!str) {
        return false;
      }

      JSAutoByteString buffer(aCx, str);
      if (!buffer) {
        return false;
      }

#ifdef ANDROID
      __android_log_print(ANDROID_LOG_INFO, "Gecko", buffer.ptr());
#endif
      fputs(buffer.ptr(), stdout);
      fflush(stdout);
    }

    return true;
  }

  static JSBool
  AtoB(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    if (!GetInstancePrivate(aCx, obj, sFunctions[7].name)) {
      return false;
    }

    jsval string;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &string)) {
      return false;
    }

    jsval result;
    if (!xpc::Base64Decode(aCx, string, &result)) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, result);
    return true;
  }

  static JSBool
  BtoA(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    if (!GetInstancePrivate(aCx, obj, sFunctions[8].name)) {
      return false;
    }

    jsval binary;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &binary)) {
      return false;
    }

    jsval result;
    if (!xpc::Base64Encode(aCx, binary, &result)) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, result);
    return true;
  }
};

JSClass WorkerGlobalScope::sClass = {
  "WorkerGlobalScope",
  0,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

JSPropertySpec WorkerGlobalScope::sProperties[] = {
  { "location", SLOT_location, PROPERTY_FLAGS, GetLocation, 
    js_GetterOnlyPropertyStub },
  { sEventStrings[STRING_onerror], STRING_onerror, PROPERTY_FLAGS,
    GetOnErrorListener, SetOnErrorListener },
  { sEventStrings[STRING_onclose], STRING_onclose, PROPERTY_FLAGS,
    _GetEventListener, _SetEventListener },
  { "navigator", SLOT_navigator, PROPERTY_FLAGS, GetNavigator, 
    js_GetterOnlyPropertyStub },
  { "self", 0, PROPERTY_FLAGS, GetSelf, js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec WorkerGlobalScope::sFunctions[] = {
  JS_FN("close", Close, 0, FUNCTION_FLAGS),
  JS_FN("importScripts", ImportScripts, 1, FUNCTION_FLAGS),
  JS_FN("setTimeout", SetTimeout, 1, FUNCTION_FLAGS),
  JS_FN("clearTimeout", ClearTimeout, 1, FUNCTION_FLAGS),
  JS_FN("setInterval", SetInterval, 1, FUNCTION_FLAGS),
  JS_FN("clearInterval", ClearTimeout, 1, FUNCTION_FLAGS),
  JS_FN("dump", Dump, 1, FUNCTION_FLAGS),
  JS_FN("atob", AtoB, 1, FUNCTION_FLAGS),
  JS_FN("btoa", BtoA, 1, FUNCTION_FLAGS),
  JS_FS_END
};

const char* const WorkerGlobalScope::sEventStrings[STRING_COUNT] = {
  "onerror",
  "onclose"
};

class DedicatedWorkerGlobalScope : public WorkerGlobalScope
{
  static DOMJSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

  enum
  {
    STRING_onmessage = 0,

    STRING_COUNT
  };

  static const char* const sEventStrings[STRING_COUNT];

public:
  static JSClass*
  Class()
  {
    return sClass.ToJSClass();
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto)
  {
    return JS_InitClass(aCx, aObj, aParentProto, Class(), Construct, 0,
                        sProperties, sFunctions, NULL, NULL);
  }

  static JSBool
  InitPrivate(JSContext* aCx, JSObject* aObj, WorkerPrivate* aWorkerPrivate)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());

    mozilla::dom::bindings::AllocateProtoOrIfaceCache(aObj);

    nsRefPtr<DedicatedWorkerGlobalScope> scope =
      new DedicatedWorkerGlobalScope(aCx, aWorkerPrivate);

    js::SetReservedSlot(aObj, DOM_GLOBAL_OBJECT_SLOT, PRIVATE_TO_JSVAL(scope));

    scope->SetIsDOMBinding();
    scope->SetWrapper(aObj);

    scope.forget();
    return true;
  }

protected:
  DedicatedWorkerGlobalScope(JSContext* aCx, WorkerPrivate* aWorker)
  : WorkerGlobalScope(aCx, aWorker)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::DedicatedWorkerGlobalScope);
  }

  ~DedicatedWorkerGlobalScope()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::DedicatedWorkerGlobalScope);
  }

private:
  static JSBool
  _GetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];
    DedicatedWorkerGlobalScope* scope = GetInstancePrivate(aCx, aObj, name);
    if (!scope) {
      return false;
    }

    nsresult rv = NS_OK;

    JSObject* listener =
      scope->GetEventListener(NS_ConvertASCIItoUTF16(name + 2), rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to get event listener!");
      return false;
    }

    *aVp = listener ? OBJECT_TO_JSVAL(listener) : JSVAL_NULL;
    return true;
  }

  static JSBool
  _SetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, JSBool aStrict,
                    jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];
    DedicatedWorkerGlobalScope* scope = GetInstancePrivate(aCx, aObj, name);
    if (!scope) {
      return false;
    }

    if (JSVAL_IS_PRIMITIVE(*aVp)) {
      JS_ReportError(aCx, "Not an event listener!");
      return false;
    }

    nsresult rv = NS_OK;

    scope->SetEventListener(NS_ConvertASCIItoUTF16(name + 2),
                            JSVAL_TO_OBJECT(*aVp), rv);

    if (NS_FAILED(rv)) {
      JS_ReportError(aCx, "Failed to set event listener!");
      return false;
    }

    return true;
  }

  static DedicatedWorkerGlobalScope*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    JSClass* classPtr = JS_GetClass(aObj);
    if (classPtr == Class()) {
      return UnwrapDOMObject<DedicatedWorkerGlobalScope>(aObj, classPtr);
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, Class()->name, aFunctionName,
                         classPtr->name);
    return NULL;
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                         Class()->name);
    return false;
  }

  static JSBool
  Resolve(JSContext* aCx, JSObject* aObj, jsid aId, unsigned aFlags,
          JSObject** aObjp)
  {
    JSBool resolved;
    if (!JS_ResolveStandardClass(aCx, aObj, aId, &resolved)) {
      return false;
    }

    *aObjp = resolved ? aObj : NULL;
    return true;
  }

  static void
  Finalize(JSFreeOp* aFop, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    DedicatedWorkerGlobalScope* scope =
      UnwrapDOMObject<DedicatedWorkerGlobalScope>(aObj, Class());
    if (scope) {
      DestroyProtoOrIfaceCache(aObj);
      scope->_Finalize(aFop);
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == Class());
    DedicatedWorkerGlobalScope* scope =
      UnwrapDOMObject<DedicatedWorkerGlobalScope>(aObj, Class());
    if (scope) {
      scope->_Trace(aTrc);
    }
  }

  static JSBool
  PostMessage(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    const char*& name = sFunctions[0].name;
    DedicatedWorkerGlobalScope* scope = GetInstancePrivate(aCx, obj, name);
    if (!scope) {
      return false;
    }

    jsval message;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &message)) {
      return false;
    }

    return scope->mWorker->PostMessageToParent(aCx, message);
  }
};

MOZ_STATIC_ASSERT(prototypes::MaxProtoChainLength == 3,
                  "The MaxProtoChainLength must match our manual DOMJSClasses");

DOMJSClass DedicatedWorkerGlobalScope::sClass = {
  {
    "DedicatedWorkerGlobalScope",
    JSCLASS_DOM_GLOBAL | JSCLASS_IS_DOMJSCLASS | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(3) | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, reinterpret_cast<JSResolveOp>(Resolve), JS_ConvertStub,
    Finalize, NULL, NULL, NULL, NULL, Trace
  },
  { prototypes::id::EventTarget_workers, prototypes::id::_ID_Count,
    prototypes::id::_ID_Count },
  -1, false, DOM_GLOBAL_OBJECT_SLOT
};

JSPropertySpec DedicatedWorkerGlobalScope::sProperties[] = {
  { sEventStrings[STRING_onmessage], STRING_onmessage, PROPERTY_FLAGS,
    _GetEventListener, _SetEventListener },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec DedicatedWorkerGlobalScope::sFunctions[] = {
  JS_FN("postMessage", PostMessage, 1, FUNCTION_FLAGS),
  JS_FS_END
};

const char* const DedicatedWorkerGlobalScope::sEventStrings[STRING_COUNT] = {
  "onmessage",
};

WorkerGlobalScope*
WorkerGlobalScope::GetInstancePrivate(JSContext* aCx, JSObject* aObj,
                                      const char* aFunctionName)
{
  JSClass* classPtr = JS_GetClass(aObj);

  // We can only make DedicatedWorkerGlobalScope, not WorkerGlobalScope, so this
  // should never happen.
  JS_ASSERT(classPtr != Class());

  if (classPtr == DedicatedWorkerGlobalScope::Class()) {
    return UnwrapDOMObject<DedicatedWorkerGlobalScope>(aObj, classPtr);
  }

  JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                       sClass.name, aFunctionName, classPtr->name);
  return NULL;
}

} /* anonymous namespace */

BEGIN_WORKERS_NAMESPACE

JSObject*
CreateDedicatedWorkerGlobalScope(JSContext* aCx)
{
  using namespace mozilla::dom::bindings::prototypes;

  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  JS_ASSERT(worker);

  JSObject* global =
    JS_NewCompartmentAndGlobalObject(aCx, DedicatedWorkerGlobalScope::Class(),
                                     GetWorkerPrincipal());
  if (!global) {
    return NULL;
  }

  JSAutoEnterCompartment ac;
  if (!ac.enter(aCx, global)) {
    return NULL;
  }

  // Make the private slots now so that all our instance checks succeed.
  if (!DedicatedWorkerGlobalScope::InitPrivate(aCx, global, worker)) {
    return NULL;
  }

  // Proto chain should be:
  //   global -> DedicatedWorkerGlobalScope
  //          -> WorkerGlobalScope
  //          -> EventTarget
  //          -> Object

  JSObject* eventTargetProto =
    EventTarget_workers::GetProtoObject(aCx, global, global);
  if (!eventTargetProto) {
    return NULL;
  }

  JSObject* scopeProto =
    WorkerGlobalScope::InitClass(aCx, global, eventTargetProto);
  if (!scopeProto) {
    return NULL;
  }

  JSObject* dedicatedScopeProto =
    DedicatedWorkerGlobalScope::InitClass(aCx, global, scopeProto);
  if (!dedicatedScopeProto) {
    return NULL;
  }

  if (!JS_SetPrototype(aCx, global, dedicatedScopeProto)) {
    return NULL;
  }

  JSObject* workerProto = worker::InitClass(aCx, global, eventTargetProto,
                                            false);
  if (!workerProto) {
    return NULL;
  }

  if (worker->IsChromeWorker() &&
      (!chromeworker::InitClass(aCx, global, workerProto, false) ||
       !DefineChromeWorkerFunctions(aCx, global))) {
    return NULL;
  }

  // Init other classes we care about.
  if (!events::InitClasses(aCx, global, false) ||
      !file::InitClasses(aCx, global) ||
      !filereadersync::InitClass(aCx, global) ||
      !exceptions::InitClasses(aCx, global) ||
      !location::InitClass(aCx, global) ||
      !imagedata::InitClass(aCx, global) ||
      !navigator::InitClass(aCx, global)) {
    return NULL;
  }

  // Init other paris-bindings.
  if (!XMLHttpRequest_workers::CreateInterfaceObjects(aCx, global, global) ||
      !XMLHttpRequestUpload_workers::CreateInterfaceObjects(aCx, global,
                                                            global)) {
    return NULL;
  }

  if (!JS_DefineProfilingFunctions(aCx, global)) {
    return NULL;
  }

  return global;
}

bool
ClassIsWorkerGlobalScope(JSClass* aClass)
{
  return WorkerGlobalScope::Class() == aClass ||
         DedicatedWorkerGlobalScope::Class() == aClass;
}

END_WORKERS_NAMESPACE
