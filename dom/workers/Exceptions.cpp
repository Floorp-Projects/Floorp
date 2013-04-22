/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Exceptions.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "mozilla/Util.h"
#include "nsDOMException.h"
#include "nsTraceRefcnt.h"

#include "WorkerInlines.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

#define CONSTANT_FLAGS \
  JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY

using namespace mozilla;
USING_WORKERS_NAMESPACE

namespace {

class DOMException : public PrivatizableBase
{
  static JSClass sClass;
  static const JSPropertySpec sProperties[];
  static const JSFunctionSpec sFunctions[];
  static const JSPropertySpec sStaticProperties[];

  enum SLOT {
    SLOT_code = 0,
    SLOT_name,
    SLOT_message,

    SLOT_COUNT
  };

public:
  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj)
  {
    JSObject* proto = JS_InitClass(aCx, aObj, NULL, &sClass, Construct, 0,
                                   sProperties, sFunctions, sStaticProperties,
                                   NULL);
    if (proto && !JS_DefineProperties(aCx, proto, sStaticProperties)) {
      return NULL;
    }

    return proto;
  }

  static JSObject*
  Create(JSContext* aCx, nsresult aNSResult);

private:
  DOMException()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::exceptions::DOMException);
  }

  ~DOMException()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::exceptions::DOMException);
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                         sClass.name);
    return false;
  }

  static void
  Finalize(JSFreeOp* aFop, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == &sClass);
    delete GetJSPrivateSafeish<DOMException>(aObj);
  }

  static JSBool
  ToString(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    JSClass* classPtr = JS_GetClass(obj);
    if (classPtr != &sClass) {
      JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                           JSMSG_INCOMPATIBLE_PROTO, sClass.name, "toString",
                           classPtr->name);
      return false;
    }

    jsval name = JS_GetReservedSlot(obj, SLOT_name);
    JS_ASSERT(name.isString());

    JSString *colon = JS_NewStringCopyN(aCx, ": ", 2);
    if (!colon){
      return false;
    }

    JSString* out = JS_ConcatStrings(aCx, name.toString(), colon);
    if (!out) {
      return false;
    }

    jsval message = JS_GetReservedSlot(obj, SLOT_message);
    JS_ASSERT(message.isString());

    out = JS_ConcatStrings(aCx, out, message.toString());
    if (!out) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, STRING_TO_JSVAL(out));
    return true;
  }

  static JSBool
  GetProperty(JSContext* aCx, JSHandleObject aObj, JSHandleId aIdval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32_t slot = JSID_TO_INT(aIdval);

    JSClass* classPtr = JS_GetClass(aObj);

    if (classPtr != &sClass || !GetJSPrivateSafeish<DOMException>(aObj)) {
      JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                           JSMSG_INCOMPATIBLE_PROTO, sClass.name,
                           sProperties[slot].name, classPtr->name);
      return false;
    }

    aVp.set(JS_GetReservedSlot(aObj, slot));
    return true;
  }

  static JSBool
  GetConstant(JSContext* aCx, JSHandleObject aObj, JSHandleId idval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(idval));
    aVp.set(INT_TO_JSVAL(JSID_TO_INT(idval)));
    return true;
  }
};

JSClass DOMException::sClass = {
  "DOMException",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

const JSPropertySpec DOMException::sProperties[] = {
  { "code", SLOT_code, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "name", SLOT_name, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "message", SLOT_message, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

const JSFunctionSpec DOMException::sFunctions[] = {
  JS_FN("toString", ToString, 0, 0),
  JS_FS_END
};

const JSPropertySpec DOMException::sStaticProperties[] = {

#define EXCEPTION_ENTRY(_name) \
  { #_name, _name, CONSTANT_FLAGS, JSOP_WRAPPER(GetConstant), JSOP_NULLWRAPPER },

  EXCEPTION_ENTRY(INDEX_SIZE_ERR)
  EXCEPTION_ENTRY(DOMSTRING_SIZE_ERR)
  EXCEPTION_ENTRY(HIERARCHY_REQUEST_ERR)
  EXCEPTION_ENTRY(WRONG_DOCUMENT_ERR)
  EXCEPTION_ENTRY(INVALID_CHARACTER_ERR)
  EXCEPTION_ENTRY(NO_DATA_ALLOWED_ERR)
  EXCEPTION_ENTRY(NO_MODIFICATION_ALLOWED_ERR)
  EXCEPTION_ENTRY(NOT_FOUND_ERR)
  EXCEPTION_ENTRY(NOT_SUPPORTED_ERR)
  EXCEPTION_ENTRY(INUSE_ATTRIBUTE_ERR)
  EXCEPTION_ENTRY(INVALID_STATE_ERR)
  EXCEPTION_ENTRY(SYNTAX_ERR)
  EXCEPTION_ENTRY(INVALID_MODIFICATION_ERR)
  EXCEPTION_ENTRY(NAMESPACE_ERR)
  EXCEPTION_ENTRY(INVALID_ACCESS_ERR)
  EXCEPTION_ENTRY(VALIDATION_ERR)
  EXCEPTION_ENTRY(TYPE_MISMATCH_ERR)
  EXCEPTION_ENTRY(SECURITY_ERR)
  EXCEPTION_ENTRY(NETWORK_ERR)
  EXCEPTION_ENTRY(ABORT_ERR)
  EXCEPTION_ENTRY(URL_MISMATCH_ERR)
  EXCEPTION_ENTRY(QUOTA_EXCEEDED_ERR)
  EXCEPTION_ENTRY(TIMEOUT_ERR)
  EXCEPTION_ENTRY(INVALID_NODE_TYPE_ERR)
  EXCEPTION_ENTRY(DATA_CLONE_ERR)

#undef EXCEPTION_ENTRY

  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

// static
JSObject*
DOMException::Create(JSContext* aCx, nsresult aNSResult)
{
  JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
  if (!obj) {
    return NULL;
  }

  const char* name;
  const char* message;
  uint16_t code;
  if (NS_FAILED(NS_GetNameAndMessageForDOMNSResult(aNSResult, &name, &message,
                                                   &code))) {
    JS_ReportError(aCx, "Exception thrown (nsresult = 0x%x).", aNSResult);
    return NULL;
  }

  JSString* jsname = JS_NewStringCopyZ(aCx, name);
  if (!jsname) {
    return NULL;
  }

  JSString* jsmessage = JS_NewStringCopyZ(aCx, message);
  if (!jsmessage) {
    return NULL;
  }

  JS_SetReservedSlot(obj, SLOT_code, INT_TO_JSVAL(code));
  JS_SetReservedSlot(obj, SLOT_name, STRING_TO_JSVAL(jsname));
  JS_SetReservedSlot(obj, SLOT_message, STRING_TO_JSVAL(jsmessage));

  DOMException* priv = new DOMException();
  SetJSPrivateSafeish(obj, priv);

  return obj;
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace exceptions {

bool
InitClasses(JSContext* aCx, JSObject* aGlobal)
{
  return DOMException::InitClass(aCx, aGlobal);
}

void
ThrowDOMExceptionForNSResult(JSContext* aCx, nsresult aNSResult)
{
  JSObject* exception = DOMException::Create(aCx, aNSResult);
  if (!exception) {
    return;
  }

  JS_SetPendingException(aCx, OBJECT_TO_JSVAL(exception));
}

} // namespace exceptions

END_WORKERS_NAMESPACE
