/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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

#include "XMLHttpRequest.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfriendapi.h"

#include "Exceptions.h"
#include "WorkerPrivate.h"
#include "XMLHttpRequestPrivate.h"

#include "WorkerInlines.h"

#define PROPERTY_FLAGS \
  JSPROP_ENUMERATE | JSPROP_SHARED

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

#define CONSTANT_FLAGS \
  JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::xhr::XMLHttpRequestPrivate;

namespace {

class XMLHttpRequestUpload : public events::EventTarget
{
  static JSClass sClass;
  static JSPropertySpec sProperties[];

  enum
  {
    STRING_onabort = 0,
    STRING_onerror,
    STRING_onload,
    STRING_onloadstart,
    STRING_onprogress,
    STRING_onloadend,

    STRING_COUNT
  };

  static const char* const sEventStrings[STRING_COUNT];

  enum SLOT {
    SLOT_xhrParent = 0,

    SLOT_COUNT
  };

public:
  static JSClass*
  Class()
  {
    return &sClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto)
  {
    return JS_InitClass(aCx, aObj, aParentProto, &sClass, Construct, 0,
                        sProperties, NULL, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, JSObject* aParentObj)
  {
    JS_ASSERT(aParentObj);

    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (obj) {
      XMLHttpRequestUpload* priv = new XMLHttpRequestUpload();
      if (!JS_SetReservedSlot(aCx, obj, SLOT_xhrParent,
                              OBJECT_TO_JSVAL(aParentObj)) ||
          !SetJSPrivateSafeish(aCx, obj, priv)) {
        delete priv;
        return NULL;
      }
    }
    return obj;
  }

  static bool
  UpdateState(JSContext* aCx, JSObject* aObj, const xhr::StateData& aNewState);

private:
  XMLHttpRequestUpload()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::xhr::XMLHttpRequestUpload);
  }

  ~XMLHttpRequestUpload()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::xhr::XMLHttpRequestUpload);
  }

  static XMLHttpRequestUpload*
  GetPrivate(JSContext* aCx, JSObject* aObj)
  {
    if (aObj) {
      JSClass* classPtr = JS_GET_CLASS(aCx, aObj);
      if (classPtr == &sClass) {
        return GetJSPrivateSafeish<XMLHttpRequestUpload>(aCx, aObj);
      }
    }
    return NULL;
  }

  static XMLHttpRequestUpload*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    JSClass* classPtr = NULL;

    if (aObj) {
      XMLHttpRequestUpload* priv = GetPrivate(aCx, aObj);
      if (priv) {
        return priv;
      }

      classPtr = JS_GET_CLASS(aCx, aObj);
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr ? classPtr->name : "object");
    return NULL;
  }

  static JSBool
  Construct(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                         sClass.name);
    return false;
  }

  static void
  Finalize(JSContext* aCx, JSObject* aObj)
  {
    JS_ASSERT(JS_GET_CLASS(aCx, aObj) == &sClass);
    XMLHttpRequestUpload* priv = GetPrivate(aCx, aObj);
    if (priv) {
      priv->FinalizeInstance(aCx);
      delete priv;
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GET_CLASS(aTrc->context, aObj) == &sClass);
    XMLHttpRequestUpload* priv = GetPrivate(aTrc->context, aObj);
    if (priv) {
      priv->TraceInstance(aTrc);
    }
  }

  static JSBool
  GetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];

    XMLHttpRequestUpload* priv = GetInstancePrivate(aCx, aObj, name);
    if (!priv) {
      return false;
    }

    return priv->GetEventListenerOnEventTarget(aCx, name + 2, aVp);
  }

  static JSBool
  SetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, JSBool aStrict,
                   jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];

    XMLHttpRequestUpload* priv = GetInstancePrivate(aCx, aObj, name);
    if (!priv) {
      return false;
    }

    return priv->SetEventListenerOnEventTarget(aCx, name + 2, aVp);
  }
};

JSClass XMLHttpRequestUpload::sClass = {
  "XMLHttpRequestUpload",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
  NULL, NULL, NULL, NULL, NULL, NULL, Trace, NULL
};

JSPropertySpec XMLHttpRequestUpload::sProperties[] = {
  { sEventStrings[STRING_onabort], STRING_onabort, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onerror], STRING_onerror, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onload], STRING_onload, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onloadstart], STRING_onloadstart, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onprogress], STRING_onprogress, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onloadend], STRING_onloadend, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { 0, 0, 0, NULL, NULL }
};

const char* const XMLHttpRequestUpload::sEventStrings[STRING_COUNT] = {
  "onabort",
  "onerror",
  "onload",
  "onloadstart",
  "onprogress",
  "onloadend"
};

class XMLHttpRequest
{
  static JSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];
  static JSPropertySpec sStaticProperties[];

  enum SLOT {
    SLOT_channel = 0,
    SLOT_responseXML,
    SLOT_responseText,
    SLOT_status,
    SLOT_statusText,
    SLOT_readyState,
    SLOT_response,
    SLOT_multipart,
    SLOT_mozBackgroundRequest,
    SLOT_withCredentials,
    SLOT_upload,
    SLOT_responseType,

    SLOT_COUNT
  };

  enum {
    UNSENT = 0,
    OPENED = 1,
    HEADERS_RECEIVED = 2,
    LOADING = 3,
    DONE = 4
  };

  enum
  {
    STRING_onreadystatechange = 0,
    STRING_onabort,
    STRING_onerror,
    STRING_onload,
    STRING_onloadstart,
    STRING_onprogress,
    STRING_onloadend,

    STRING_COUNT
  };

  static const char* const sEventStrings[STRING_COUNT];

public:
  static JSClass*
  Class()
  {
    return &sClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto)
  {
    JSObject* proto = JS_InitClass(aCx, aObj, aParentProto, &sClass, Construct,
                                   0, sProperties, sFunctions,
                                   sStaticProperties, NULL);
    if (proto && !JS_DefineProperties(aCx, proto, sStaticProperties)) {
      return NULL;
    }

    return proto;
  }

  static bool
  UpdateState(JSContext* aCx, JSObject* aObj, const xhr::StateData& aNewState)
  {
    JS_ASSERT(GetPrivate(aCx, aObj));

#define HANDLE_STATE_VALUE(_member, _slot) \
  if (aNewState. _member##Exception || !JSVAL_IS_VOID(aNewState. _member)) { \
    if (!JS_SetReservedSlot(aCx, aObj, _slot, aNewState. _member)) { \
      return false; \
    } \
  }

    HANDLE_STATE_VALUE(mResponseText, SLOT_responseText)
    HANDLE_STATE_VALUE(mStatus, SLOT_status)
    HANDLE_STATE_VALUE(mStatusText, SLOT_statusText)
    HANDLE_STATE_VALUE(mReadyState, SLOT_readyState)
    HANDLE_STATE_VALUE(mResponse, SLOT_response)

#undef HANDLE_STATE_VALUE

    return true;
  }

private:
  // No instance of this class should ever be created so these are explicitly
  // left without an implementation to prevent linking in case someone tries to
  // make one.
  XMLHttpRequest();
  ~XMLHttpRequest();

  static XMLHttpRequestPrivate*
  GetPrivate(JSContext* aCx, JSObject* aObj)
  {
    if (aObj) {
      JSClass* classPtr = JS_GET_CLASS(aCx, aObj);
      if (classPtr == &sClass) {
        return GetJSPrivateSafeish<XMLHttpRequestPrivate>(aCx, aObj);
      }
    }
    return NULL;
  }

  static XMLHttpRequestPrivate*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    JSClass* classPtr = NULL;

    if (aObj) {
      XMLHttpRequestPrivate* priv = GetPrivate(aCx, aObj);
      if (priv) {
        return priv;
      }

      classPtr = JS_GET_CLASS(aCx, aObj);
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr ? classPtr->name : "object");
    return NULL;
  }

  static JSBool
  Construct(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (!obj) {
      return false;
    }

    JSString* textStr = JS_NewStringCopyN(aCx, "text", 4);
    if (!textStr) {
      return false;
    }

    jsval emptyString = JS_GetEmptyStringValue(aCx);
    jsval zero = INT_TO_JSVAL(0);

    if (!JS_SetReservedSlot(aCx, obj, SLOT_channel, JSVAL_NULL) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_responseXML, JSVAL_NULL) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_responseText, emptyString) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_status, zero) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_statusText, emptyString) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_readyState, zero) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_multipart, JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_mozBackgroundRequest, JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_withCredentials, JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_upload, JSVAL_NULL) ||
        !JS_SetReservedSlot(aCx, obj, SLOT_responseType,
                            STRING_TO_JSVAL(textStr))) {
      return false;
    }

    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    XMLHttpRequestPrivate* priv = new XMLHttpRequestPrivate(obj, workerPrivate);
    if (!SetJSPrivateSafeish(aCx, obj, priv)) {
      delete priv;
      return false;
    }

    JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(obj));
    return true;
  }

  static void
  Finalize(JSContext* aCx, JSObject* aObj)
  {
    JS_ASSERT(JS_GET_CLASS(aCx, aObj) == &sClass);
    XMLHttpRequestPrivate* priv = GetPrivate(aCx, aObj);
    if (priv) {
      priv->FinalizeInstance(aCx);
      delete priv;
    }
  }

  static void
  Trace(JSTracer* aTrc, JSObject* aObj)
  {
    JS_ASSERT(JS_GET_CLASS(aTrc->context, aObj) == &sClass);
    XMLHttpRequestPrivate* priv = GetPrivate(aTrc->context, aObj);
    if (priv) {
      priv->TraceInstance(aTrc);
    }
  }

  static JSBool
  GetProperty(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32 slot = JSID_TO_INT(aIdval);
    const char*& name = sProperties[slot].name;

    if (!GetInstancePrivate(aCx, aObj, name)) {
      return false;
    }

    jsval rval;
    if (!JS_GetReservedSlot(aCx, aObj, slot, &rval)) {
      return false;
    }

    if (JSVAL_IS_VOID(rval)) {
      // Throw an exception.
      exceptions::ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
      return false;
    }

    *aVp = rval;
    return true;
  }

  static JSBool
  GetConstant(JSContext* aCx, JSObject* aObj, jsid idval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(idval));
    JS_ASSERT(JSID_TO_INT(idval) >= UNSENT &&
              JSID_TO_INT(idval) <= DONE);

    *aVp = INT_TO_JSVAL(JSID_TO_INT(idval));
    return true;
  }

  static JSBool
  GetUpload(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32 slot = JSID_TO_INT(aIdval);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, aObj, sProperties[slot].name);
    if (!priv) {
      return false;
    }

    jsval uploadVal;
    if (!JS_GetReservedSlot(aCx, aObj, slot, &uploadVal)) {
      return false;
    }

    if (JSVAL_IS_NULL(uploadVal)) {
      JSObject* uploadObj = XMLHttpRequestUpload::Create(aCx, aObj);
      if (!uploadObj) {
        return false;
      }

      uploadVal = OBJECT_TO_JSVAL(uploadObj);

      if (!JS_SetReservedSlot(aCx, aObj, slot, uploadVal)) {
        return false;
      }

      priv->SetUploadObject(uploadObj);
    }

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(uploadVal));

    *aVp = uploadVal;
    return true;
  }

#define IMPL_SETTER(_name)                                                     \
  static JSBool                                                                \
  Set##_name (JSContext* aCx, JSObject* aObj, jsid aIdval, JSBool aStrict,     \
              jsval* aVp)                                                      \
  {                                                                            \
    JS_ASSERT(JSID_IS_INT(aIdval));                                            \
                                                                               \
    int32 slot = JSID_TO_INT(aIdval);                                          \
                                                                               \
    XMLHttpRequestPrivate* priv =                                              \
      GetInstancePrivate(aCx, aObj, sProperties[slot].name);                   \
    if (!priv) {                                                               \
      return false;                                                            \
    }                                                                          \
                                                                               \
    jsval oldVal;                                                              \
    if (!JS_GetReservedSlot(aCx, aObj, slot, &oldVal)) {                       \
      return false;                                                            \
    }                                                                          \
                                                                               \
    jsval rval = *aVp;                                                         \
    if (!priv->Set##_name (aCx, oldVal, &rval) ||                              \
        !JS_SetReservedSlot(aCx, aObj, slot, rval)) {                          \
      return false;                                                            \
    }                                                                          \
                                                                               \
    *aVp = rval;                                                               \
    return true;                                                               \
  }

  IMPL_SETTER(Multipart)
  IMPL_SETTER(MozBackgroundRequest)
  IMPL_SETTER(WithCredentials)
  IMPL_SETTER(ResponseType)

#undef IMPL_SETTER

  static JSBool
  GetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];

    XMLHttpRequestPrivate* priv = GetInstancePrivate(aCx, aObj, name);
    if (!priv) {
      return false;
    }

    return priv->GetEventListenerOnEventTarget(aCx, name + 2, aVp);
  }

  static JSBool
  SetEventListener(JSContext* aCx, JSObject* aObj, jsid aIdval, JSBool aStrict,
                   jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < STRING_COUNT);

    const char* name = sEventStrings[JSID_TO_INT(aIdval)];

    XMLHttpRequestPrivate* priv = GetInstancePrivate(aCx, aObj, name);
    if (!priv) {
      return false;
    }

    return priv->SetEventListenerOnEventTarget(aCx, name + 2, aVp);
  }

  static JSBool
  Abort(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!priv) {
      return false;
    }

    return priv->Abort(aCx);
  }

  static JSBool
  GetAllResponseHeaders(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[1].name);
    if (!priv) {
      return false;
    }

    JSString* responseHeaders = priv->GetAllResponseHeaders(aCx);
    if (!responseHeaders) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, STRING_TO_JSVAL(responseHeaders));
    return true;
  }

  static JSBool
  GetResponseHeader(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[2].name);
    if (!priv) {
      return false;
    }

    jsval headerVal;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &headerVal)) {
      return false;
    }

    JSString* header;
    if (JSVAL_IS_NULL(headerVal)) {
      header = JSVAL_TO_STRING(JS_GetEmptyStringValue(aCx));
    }
    else {
      header = JS_ValueToString(aCx, headerVal);
      if (!header) {
        return false;
      }
    }

    JSString* value = priv->GetResponseHeader(aCx, header);
    if (!value) {
      return false;
    }
  
    JS_SET_RVAL(aCx, aVp, STRING_TO_JSVAL(value));
    return true;
  }

  static JSBool
  Open(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[3].name);
    if (!priv) {
      return false;
    }

    JSString* method, *url;
    JSBool async = true;
    JSString* user = JS_GetEmptyString(JS_GetRuntime(aCx));
    JSString* password = user;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SS/bSS", &method,
                             &url, &async, &user, &password)) {
      return false;
    }

    return priv->Open(aCx, method, url, async, user, password);
  }

  static JSBool
  Send(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[4].name);
    if (!priv) {
      return false;
    }

    jsval body = aArgc ? JS_ARGV(aCx, aVp)[0] : JSVAL_VOID;

    return priv->Send(aCx, !!aArgc, body);
  }

  static JSBool
  SendAsBinary(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[5].name);
    if (!priv) {
      return false;
    }

    jsval bodyVal;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &bodyVal)) {
      return false;
    }

    JSString* body;
    if (JSVAL_IS_NULL(bodyVal)) {
      body = JSVAL_TO_STRING(JS_GetEmptyStringValue(aCx));
    }
    else {
      body = JS_ValueToString(aCx, bodyVal);
      if (!body) {
        return false;
      }
    }

    return priv->SendAsBinary(aCx, body);
  }

  static JSBool
  SetRequestHeader(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[6].name);
    if (!priv) {
      return false;
    }

    JSString* header, *value;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SS", &header,
                             &value)) {
      return false;
    }

    return priv->SetRequestHeader(aCx, header, value);
  }

  static JSBool
  OverrideMimeType(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    XMLHttpRequestPrivate* priv =
      GetInstancePrivate(aCx, obj, sFunctions[7].name);
    if (!priv) {
      return false;
    }

    JSString* mimeType;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "S", &mimeType)) {
      return false;
    }

    return priv->OverrideMimeType(aCx, mimeType);
  }
};

JSClass XMLHttpRequest::sClass = {
  "XMLHttpRequest",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
  NULL, NULL, NULL, NULL, NULL, NULL, Trace, NULL
};

JSPropertySpec XMLHttpRequest::sProperties[] = {

#define GENERIC_READONLY_PROPERTY(_name) \
  { #_name, SLOT_##_name, PROPERTY_FLAGS, GetProperty, \
    js_GetterOnlyPropertyStub },

  GENERIC_READONLY_PROPERTY(channel)
  GENERIC_READONLY_PROPERTY(responseXML)
  GENERIC_READONLY_PROPERTY(responseText)
  GENERIC_READONLY_PROPERTY(status)
  GENERIC_READONLY_PROPERTY(statusText)
  GENERIC_READONLY_PROPERTY(readyState)
  GENERIC_READONLY_PROPERTY(response)

  { "multipart", SLOT_multipart, PROPERTY_FLAGS, GetProperty, SetMultipart },
  { "mozBackgroundRequest", SLOT_mozBackgroundRequest, PROPERTY_FLAGS,
    GetProperty, SetMozBackgroundRequest },
  { "withCredentials", SLOT_withCredentials, PROPERTY_FLAGS, GetProperty,
    SetWithCredentials },
  { "upload", SLOT_upload, PROPERTY_FLAGS, GetUpload,
    js_GetterOnlyPropertyStub },
  { "responseType", SLOT_responseType, PROPERTY_FLAGS, GetProperty,
    SetResponseType },
  { sEventStrings[STRING_onreadystatechange], STRING_onreadystatechange,
    PROPERTY_FLAGS, GetEventListener, SetEventListener },
  { sEventStrings[STRING_onabort], STRING_onabort, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onerror], STRING_onerror, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onload], STRING_onload, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onloadstart], STRING_onloadstart, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onprogress], STRING_onprogress, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },
  { sEventStrings[STRING_onloadend], STRING_onloadend, PROPERTY_FLAGS,
    GetEventListener, SetEventListener },

#undef GENERIC_READONLY_PROPERTY

  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec XMLHttpRequest::sFunctions[] = {
  JS_FN("abort", Abort, 0, FUNCTION_FLAGS),
  JS_FN("getAllResponseHeaders", GetAllResponseHeaders, 0, FUNCTION_FLAGS),
  JS_FN("getResponseHeader", GetResponseHeader, 1, FUNCTION_FLAGS),
  JS_FN("open", Open, 2, FUNCTION_FLAGS),
  JS_FN("send", Send, 0, FUNCTION_FLAGS),
  JS_FN("sendAsBinary", SendAsBinary, 1, FUNCTION_FLAGS),
  JS_FN("setRequestHeader", SetRequestHeader, 2, FUNCTION_FLAGS),
  JS_FN("overrideMimeType", OverrideMimeType, 1, FUNCTION_FLAGS),
  JS_FS_END
};

JSPropertySpec XMLHttpRequest::sStaticProperties[] = {
  { "UNSENT", UNSENT, CONSTANT_FLAGS, GetConstant, NULL },
  { "OPENED", OPENED, CONSTANT_FLAGS, GetConstant, NULL },
  { "HEADERS_RECEIVED", HEADERS_RECEIVED, CONSTANT_FLAGS, GetConstant, NULL },
  { "LOADING", LOADING, CONSTANT_FLAGS, GetConstant, NULL },
  { "DONE", DONE, CONSTANT_FLAGS, GetConstant, NULL },
  { 0, 0, 0, NULL, NULL }
};

const char* const XMLHttpRequest::sEventStrings[STRING_COUNT] = {
  "onreadystatechange",
  "onabort",
  "onerror",
  "onload",
  "onloadstart",
  "onprogress",
  "onloadend"
};

// static
bool
XMLHttpRequestUpload::UpdateState(JSContext* aCx, JSObject* aObj,
                                  const xhr::StateData& aNewState)
{
  JS_ASSERT(JS_GET_CLASS(aCx, aObj) == &sClass);

  jsval parentVal;
  if (!JS_GetReservedSlot(aCx, aObj, SLOT_xhrParent, &parentVal)) {
    return false;
  }

  if (!JSVAL_IS_PRIMITIVE(parentVal)) {
    return XMLHttpRequest::UpdateState(aCx, JSVAL_TO_OBJECT(parentVal),
                                       aNewState);
  }

  return true;
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace xhr {

bool
InitClasses(JSContext* aCx, JSObject* aGlobal, JSObject* aProto)
{
  return XMLHttpRequest::InitClass(aCx, aGlobal, aProto) &&
         XMLHttpRequestUpload::InitClass(aCx, aGlobal, aProto);
}

bool
UpdateXHRState(JSContext* aCx, JSObject* aObj, bool aIsUpload,
               const StateData& aNewState)
{
  return aIsUpload ?
         XMLHttpRequestUpload::UpdateState(aCx, aObj, aNewState) :
         XMLHttpRequest::UpdateState(aCx, aObj, aNewState);
}

} // namespace xhr

END_WORKERS_NAMESPACE
