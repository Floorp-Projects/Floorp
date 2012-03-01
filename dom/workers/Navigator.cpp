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
  static JSPropertySpec sProperties[];

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

    JSString* appName, *version, *platform, *userAgent;

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
  Finalize(JSContext* aCx, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == &sClass);
    delete static_cast<Navigator*>(JS_GetPrivate(aObj));
  }

  static JSBool
  GetProperty(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
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

    *aVp = JS_GetReservedSlot(aObj, JSID_TO_INT(aIdval));
    return true;
  }
};

JSClass Navigator::sClass = {
  "WorkerNavigator",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

JSPropertySpec Navigator::sProperties[] = {
  { "appName", SLOT_appName, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { "appVersion", SLOT_appVersion, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { "platform", SLOT_platform, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { "userAgent", SLOT_userAgent, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
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
