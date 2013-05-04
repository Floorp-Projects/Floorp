/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Navigator.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsTraceRefcnt.h"

#include "RuntimeService.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

USING_WORKERS_NAMESPACE

namespace {

class Navigator
{
  static JSClass sClass;
  static const JSPropertySpec sProperties[];

  enum SLOT {
    SLOT_appName = 0,
    SLOT_appVersion,
    SLOT_platform,
    SLOT_userAgent,

    SLOT_COUNT
  };

public:
  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj)
  {
    return JS_InitClass(aCx, aObj, NULL, &sClass, Construct, 0, sProperties,
                        NULL, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx)
  {
    RuntimeService* rts = RuntimeService::GetService();
    JS_ASSERT(rts);

    const RuntimeService::NavigatorStrings& strings =
      rts->GetNavigatorStrings();

    JS::Rooted<JSString*> appName(aCx), version(aCx), platform(aCx), userAgent(aCx);

#define COPY_STRING(_jsstr, _str)                                              \
  if (strings. _str .IsEmpty()) {                                              \
    _jsstr = NULL;                                                             \
  }                                                                            \
  else if (!(_jsstr = JS_NewUCStringCopyN(aCx, strings. _str .get(),           \
                                          strings. _str .Length()))) {         \
    return NULL;                                                               \
  }

    COPY_STRING(appName, mAppName);
    COPY_STRING(version, mAppVersion);
    COPY_STRING(platform, mPlatform);
    COPY_STRING(userAgent, mUserAgent);

#undef COPY_STRING

    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (!obj) {
      return NULL;
    }

    jsval empty = JS_GetEmptyStringValue(aCx);

    JS_SetReservedSlot(obj, SLOT_appName,
                       appName ? STRING_TO_JSVAL(appName) : empty);
    JS_SetReservedSlot(obj, SLOT_appVersion,
                       version ? STRING_TO_JSVAL(version) : empty);
    JS_SetReservedSlot(obj, SLOT_platform,
                       platform ? STRING_TO_JSVAL(platform) : empty);
    JS_SetReservedSlot(obj, SLOT_userAgent,
                       userAgent ? STRING_TO_JSVAL(userAgent) : empty);

    Navigator* priv = new Navigator();
    JS_SetPrivate(obj, priv);

    return obj;
  }

private:
  Navigator()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::Navigator);
  }

  ~Navigator()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::Navigator);
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
    delete static_cast<Navigator*>(JS_GetPrivate(aObj));
  }

  static JSBool
  GetProperty(JSContext* aCx, JSHandleObject aObj, JSHandleId aIdval, JSMutableHandleValue aVp)
  {
    JSClass* classPtr = JS_GetClass(aObj);
    if (classPtr != &sClass) {
      JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                           JSMSG_INCOMPATIBLE_PROTO, sClass.name, "GetProperty",
                           classPtr->name);
      return false;
    }

    JS_ASSERT(JSID_IS_INT(aIdval));
    JS_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < SLOT_COUNT);

    aVp.set(JS_GetReservedSlot(aObj, JSID_TO_INT(aIdval)));
    return true;
  }
};

JSClass Navigator::sClass = {
  "WorkerNavigator",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

const JSPropertySpec Navigator::sProperties[] = {
  { "appName", SLOT_appName, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "appVersion", SLOT_appVersion, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "platform", SLOT_platform, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "userAgent", SLOT_userAgent, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace navigator {

bool
InitClass(JSContext* aCx, JSObject* aGlobal)
{
  return !!Navigator::InitClass(aCx, aGlobal);
}

JSObject*
Create(JSContext* aCx)
{
  return Navigator::Create(aCx);
}

} // namespace navigator

END_WORKERS_NAMESPACE
