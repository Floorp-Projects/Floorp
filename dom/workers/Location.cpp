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

#include "Location.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsTraceRefcnt.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

USING_WORKERS_NAMESPACE

namespace {

class Location
{
  static JSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

  enum SLOT {
    SLOT_href = 0,
    SLOT_protocol,
    SLOT_host,
    SLOT_hostname,
    SLOT_port,
    SLOT_pathname,
    SLOT_search,
    SLOT_hash,

    SLOT_COUNT
  };

public:
  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj)
  {
    return JS_InitClass(aCx, aObj, NULL, &sClass, Construct, 0, sProperties,
                        sFunctions, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, JSString* aHref, JSString* aProtocol, JSString* aHost,
         JSString* aHostname, JSString* aPort, JSString* aPathname,
         JSString* aSearch, JSString* aHash)
  {
    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (!obj) {
      return NULL;
    }

    jsval empty = JS_GetEmptyStringValue(aCx);

    JS_SetReservedSlot(obj, SLOT_href,
                       aHref ? STRING_TO_JSVAL(aHref) : empty);
    JS_SetReservedSlot(obj, SLOT_protocol,
                       aProtocol ? STRING_TO_JSVAL(aProtocol) : empty);
    JS_SetReservedSlot(obj, SLOT_host,
                       aHost ? STRING_TO_JSVAL(aHost) : empty);
    JS_SetReservedSlot(obj, SLOT_hostname,
                       aHostname ? STRING_TO_JSVAL(aHostname) : empty);
    JS_SetReservedSlot(obj, SLOT_port,
                       aPort ? STRING_TO_JSVAL(aPort) : empty);
    JS_SetReservedSlot(obj, SLOT_pathname,
                       aPathname ? STRING_TO_JSVAL(aPathname) : empty);
    JS_SetReservedSlot(obj, SLOT_search,
                       aSearch ? STRING_TO_JSVAL(aSearch) : empty);
    JS_SetReservedSlot(obj, SLOT_hash,
                       aHash ? STRING_TO_JSVAL(aHash) : empty);

    Location* priv = new Location();
    JS_SetPrivate(obj, priv);

    return obj;
  }

private:
  Location()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::Location);
  }

  ~Location()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::Location);
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
    delete static_cast<Location*>(JS_GetPrivate(aObj));
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
                           classPtr);
      return false;
    }

    jsval href = JS_GetReservedSlot(obj, SLOT_href);

    JS_SET_RVAL(aCx, aVp, href);
    return true;
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

JSClass Location::sClass = {
  "WorkerLocation",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

JSPropertySpec Location::sProperties[] = {
  { "href", SLOT_href, PROPERTY_FLAGS, GetProperty, js_GetterOnlyPropertyStub },
  { "protocol", SLOT_protocol, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { "host", SLOT_host, PROPERTY_FLAGS, GetProperty, js_GetterOnlyPropertyStub },
  { "hostname", SLOT_hostname, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { "port", SLOT_port, PROPERTY_FLAGS, GetProperty, js_GetterOnlyPropertyStub },
  { "pathname", SLOT_pathname, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "search", SLOT_search, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "hash", SLOT_hash, PROPERTY_FLAGS, GetProperty, js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec Location::sFunctions[] = {
  JS_FN("toString", ToString, 0, 0),
  JS_FS_END
};

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace location {

bool
InitClass(JSContext* aCx, JSObject* aGlobal)
{
  return !!Location::InitClass(aCx, aGlobal);
}

JSObject*
Create(JSContext* aCx, JSString* aHref, JSString* aProtocol, JSString* aHost,
       JSString* aHostname, JSString* aPort, JSString* aPathname,
       JSString* aSearch, JSString* aHash)
{
  return Location::Create(aCx, aHref, aProtocol, aHost, aHostname, aPort,
                          aPathname, aSearch, aHash);
}

} // namespace location

END_WORKERS_NAMESPACE
