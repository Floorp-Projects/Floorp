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

#include "EventTarget.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "nsTraceRefcnt.h"

#include "WorkerInlines.h"

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::events::EventTarget;

namespace {

#define DECL_EVENTTARGET_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, 0, \
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub, \
    JSCLASS_NO_OPTIONAL_MEMBERS \
  };

DECL_EVENTTARGET_CLASS(gClass, "EventTarget")
DECL_EVENTTARGET_CLASS(gMainThreadClass, "WorkerEventTarget")

#undef DECL_EVENTTARGET_CLASS

inline
EventTarget*
GetPrivate(JSContext* aCx, JSObject* aObj)
{
  return GetJSPrivateSafeish<EventTarget>(aCx, aObj);
}

JSBool
Construct(JSContext* aCx, uintN aArgc, jsval* aVp)
{
  JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                       gClass.name);
  return false;
}

JSFunctionSpec gFunctions[] = {
  JS_FN("addEventListener", EventTarget::AddEventListener, 3, JSPROP_ENUMERATE),
  JS_FN("removeEventListener", EventTarget::RemoveEventListener, 3,
        JSPROP_ENUMERATE),
  JS_FN("dispatchEvent", EventTarget::DispatchEvent, 1, JSPROP_ENUMERATE),
  JS_FS_END
};

} // anonymous namespace

EventTarget::EventTarget()
{
  MOZ_COUNT_CTOR(mozilla::dom::workers::events::EventTarget);
}

EventTarget::~EventTarget()
{
  MOZ_COUNT_DTOR(mozilla::dom::workers::events::EventTarget);
}

bool
EventTarget::GetEventListenerOnEventTarget(JSContext* aCx, const char* aType,
                                           jsval* aVp)
{
  JSString* type = JS_InternString(aCx, aType);
  if (!type) {
    return false;
  }

  return mListenerManager.GetEventListener(aCx, type, aVp);
}

bool
EventTarget::SetEventListenerOnEventTarget(JSContext* aCx, const char* aType,
                                           jsval* aVp)
{
  JSString* type = JS_InternString(aCx, aType);
  if (!type) {
    return false;
  }

  JSObject* listenerObj;
  if (!JS_ValueToObject(aCx, *aVp, &listenerObj)) {
    return false;
  }

  return mListenerManager.SetEventListener(aCx, type, *aVp);
}

// static
EventTarget*
EventTarget::FromJSObject(JSContext* aCx, JSObject* aObj)
{
  return GetPrivate(aCx, aObj);
}

// static
JSBool
EventTarget::AddEventListener(JSContext* aCx, uintN aArgc, jsval* aVp)
{
  JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
  if (!obj) {
    return true;
  }

  EventTarget* self = GetPrivate(aCx, obj);
  if (!self) {
    return true;
  }

  JSString* type;
  JSObject* listener;
  JSBool capturing = false, wantsUntrusted = false;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "So/bb", &type,
                           &listener, &capturing, &wantsUntrusted)) {
    return false;
  }

  if (!listener) {
    return true;
  }

  return self->mListenerManager.AddEventListener(aCx, type,
                                                 JS_ARGV(aCx, aVp)[1],
                                                 capturing, wantsUntrusted);
}

// static
JSBool
EventTarget::RemoveEventListener(JSContext* aCx, uintN aArgc, jsval* aVp)
{
  JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
  if (!obj) {
    return true;
  }

  EventTarget* self = GetPrivate(aCx, obj);
  if (!self) {
    return true;
  }

  JSString* type;
  JSObject* listener;
  JSBool capturing = false;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "So/b", &type,
                           &listener, &capturing)) {
    return false;
  }

  if (!listener) {
    return true;
  }

  return self->mListenerManager.RemoveEventListener(aCx, type,
                                                    JS_ARGV(aCx, aVp)[1],
                                                    capturing);
}

// static
JSBool
EventTarget::DispatchEvent(JSContext* aCx, uintN aArgc, jsval* aVp)
{
  JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
  if (!obj) {
    return true;
  }

  EventTarget* self = GetPrivate(aCx, obj);
  if (!self) {
    return true;
  }

  JSObject* event;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "o", &event)) {
    return false;
  }

  bool preventDefaultCalled;
  if (!self->mListenerManager.DispatchEvent(aCx, obj, event,
                                            &preventDefaultCalled)) {
    return false;
  }

  JS_SET_RVAL(aCx, aVp, BOOLEAN_TO_JSVAL(preventDefaultCalled));
  return true;
}

BEGIN_WORKERS_NAMESPACE

namespace events {

JSObject*
InitEventTargetClass(JSContext* aCx, JSObject* aObj, bool aMainRuntime)
{
  if (aMainRuntime) {
    // XXX Hack to prevent instanceof checks failing on the main thread.
    jsval windowEventTarget;
    if (!JS_GetProperty(aCx, aObj, gClass.name, &windowEventTarget)) {
      return NULL;
    }

    if (!JSVAL_IS_PRIMITIVE(windowEventTarget)) {
      jsval protoVal;
      if (!JS_GetProperty(aCx, JSVAL_TO_OBJECT(windowEventTarget), "prototype",
                          &protoVal)) {
        return NULL;
      }

      if (!JSVAL_IS_PRIMITIVE(protoVal)) {
        return JS_InitClass(aCx, aObj, JSVAL_TO_OBJECT(protoVal),
                            &gMainThreadClass, Construct, 0, NULL, gFunctions,
                            NULL, NULL);
      }
    }
  }

  return JS_InitClass(aCx, aObj, NULL, &gClass, Construct, 0, NULL,
                      gFunctions, NULL, NULL);
}

} // namespace events

END_WORKERS_NAMESPACE
