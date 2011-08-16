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

#include "Events.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "nsTraceRefcnt.h"

#include "WorkerInlines.h"
#include "WorkerPrivate.h"

#define PROPERTY_FLAGS \
  JSPROP_ENUMERATE | JSPROP_SHARED

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

#define CONSTANT_FLAGS \
  JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY

USING_WORKERS_NAMESPACE

namespace {

class Event : public PrivatizableBase
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];
  static JSPropertySpec sStaticProperties[];

protected:
  bool mStopPropagationCalled;

public:
  static bool
  IsThisClass(JSClass* aClass)
  {
    return aClass == &sClass || aClass == &sMainRuntimeClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, bool aMainRuntime)
  {
    JSObject* parentProto = NULL;

    if (aMainRuntime) {
      jsval windowPropVal;
      if (!JS_GetProperty(aCx, aObj, sClass.name, &windowPropVal)) {
        return NULL;
      }

      if (!JSVAL_IS_PRIMITIVE(windowPropVal)) {
        jsval protoVal;
        if (!JS_GetProperty(aCx, JSVAL_TO_OBJECT(windowPropVal), "prototype",
                            &protoVal)) {
          return NULL;
        }

        if (!JSVAL_IS_PRIMITIVE(protoVal)) {
          parentProto = JSVAL_TO_OBJECT(protoVal);
        }
      }
    }

    JSClass* clasp = parentProto ? &sMainRuntimeClass : &sClass;

    JSObject* proto = JS_InitClass(aCx, aObj, parentProto, clasp, Construct, 0,
                                   sProperties, sFunctions, sStaticProperties,
                                   NULL);
    if (proto && !JS_DefineProperties(aCx, proto, sStaticProperties)) {
      return NULL;
    }

    return proto;
  }

  static JSObject*
  Create(JSContext* aCx, JSObject* aParent, JSString* aType, bool aBubbles,
         bool aCancelable, bool aMainRuntime)
  {
    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    JSObject* obj = JS_NewObject(aCx, clasp, NULL, aParent);
    if (obj) {
      Event* priv = new Event();
      if (!SetJSPrivateSafeish(aCx, obj, priv) ||
          !InitEventCommon(aCx, obj, priv, aType, aBubbles, aCancelable,
                           true)) {
        SetJSPrivateSafeish(aCx, obj, NULL);
        delete priv;
        return NULL;
      }
    }
    return obj;
  }

  static bool
  IsSupportedClass(JSContext* aCx, JSObject* aEvent)
  {
    return !!GetPrivate(aCx, aEvent);
  }

  static bool
  SetTarget(JSContext* aCx, JSObject* aEvent, JSObject* aTarget)
  {
    JS_ASSERT(IsSupportedClass(aCx, aEvent));

    jsval target = OBJECT_TO_JSVAL(aTarget);
    return JS_SetReservedSlot(aCx, aEvent, SLOT_target, target) &&
           JS_SetReservedSlot(aCx, aEvent, SLOT_currentTarget, target);
  }

  static bool
  WasCanceled(JSContext* aCx, JSObject* aEvent)
  {
    JS_ASSERT(IsSupportedClass(aCx, aEvent));

    jsval canceled;
    if (!GetPropertyCommon(aCx, aEvent, SLOT_defaultPrevented, &canceled)) {
      return false;
    }

    return JSVAL_TO_BOOLEAN(canceled);
  }

protected:
  Event()
  : mStopPropagationCalled(false)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::Event);
  }

  virtual ~Event()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::Event);
  }

  enum {
    CAPTURING_PHASE = 1,
    AT_TARGET = 2,
    BUBBLING_PHASE = 3
  };

  enum SLOT {
    SLOT_type = 0,
    SLOT_target,
    SLOT_currentTarget,
    SLOT_eventPhase,
    SLOT_bubbles,
    SLOT_cancelable,
    SLOT_timeStamp,
    SLOT_defaultPrevented,
    SLOT_isTrusted,

    SLOT_COUNT,
    SLOT_FIRST = SLOT_type
  };

  static Event*
  GetPrivate(JSContext* aCx, JSObject* aEvent);

  static Event*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    JSClass* classPtr = NULL;

    if (aObj) {
      Event* priv = GetPrivate(aCx, aObj);
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
  InitEventCommon(JSContext* aCx, JSObject* aObj, Event* aEvent,
                  JSString* aType, JSBool aBubbles, JSBool aCancelable,
                  bool aIsTrusted)
  {
    aEvent->mStopPropagationCalled = false;

    jsval now;
    if (!JS_NewNumberValue(aCx, JS_Now(), &now)) {
      return false;
    }

    if (!JS_SetReservedSlot(aCx, aObj, SLOT_type, STRING_TO_JSVAL(aType)) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_target, JSVAL_NULL) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_currentTarget, JSVAL_NULL) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_eventPhase,
                            INT_TO_JSVAL(CAPTURING_PHASE)) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_bubbles,
                            aBubbles ? JSVAL_TRUE : JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_cancelable,
                            aCancelable ? JSVAL_TRUE : JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_timeStamp, now) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_defaultPrevented, JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_isTrusted,
                            aIsTrusted ? JSVAL_TRUE : JSVAL_FALSE)) {
      return false;
    }

    return true;
  }

  static JSBool
  GetPropertyCommon(JSContext* aCx, JSObject* aObj, int32 aSlot, jsval* aVp)
  {
    JS_ASSERT(aSlot >= 0 && aSlot < SLOT_COUNT);
    return JS_GetReservedSlot(aCx, aObj, aSlot, aVp);
  }

private:
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
    JS_ASSERT(IsThisClass(JS_GET_CLASS(aCx, aObj)));
    delete GetJSPrivateSafeish<Event>(aCx, aObj);
  }

  static JSBool
  GetProperty(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32 slot = JSID_TO_INT(aIdval);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    if (!GetInstancePrivate(aCx, aObj, name)) {
      return false;
    }

    return GetPropertyCommon(aCx, aObj, slot, aVp);
  }

  static JSBool
  GetConstant(JSContext* aCx, JSObject* aObj, jsid idval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(idval));
    JS_ASSERT(JSID_TO_INT(idval) >= CAPTURING_PHASE &&
              JSID_TO_INT(idval) <= BUBBLING_PHASE);

    *aVp = INT_TO_JSVAL(JSID_TO_INT(idval));
    return true;
  }

  static JSBool
  StopPropagation(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    event->mStopPropagationCalled = true;

    return true;
  }

  static JSBool
  PreventDefault(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[1].name);
    if (!event) {
      return false;
    }

    jsval cancelableVal;
    if (!GetPropertyCommon(aCx, obj, SLOT_cancelable, &cancelableVal)) {
      return false;
    }

    return JSVAL_TO_BOOLEAN(cancelableVal) ?
           JS_SetReservedSlot(aCx, obj, SLOT_defaultPrevented, cancelableVal) :
           true;
  }

  static JSBool
  InitEvent(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[2].name);
    if (!event) {
      return false;
    }

    JSString* type;
    JSBool bubbles, cancelable;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "Sbb", &type,
                             &bubbles, &cancelable)) {
      return false;
    }

    return InitEventCommon(aCx, obj, event, type, bubbles, cancelable, false);
  }
};

#define DECL_EVENT_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, \
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT), \
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize, \
    JSCLASS_NO_OPTIONAL_MEMBERS \
  };

DECL_EVENT_CLASS(Event::sClass, "Event")
DECL_EVENT_CLASS(Event::sMainRuntimeClass, "WorkerEvent")

#undef DECL_EVENT_CLASS

JSPropertySpec Event::sProperties[] = {
  { "type", SLOT_type, PROPERTY_FLAGS, GetProperty, js_GetterOnlyPropertyStub },
  { "target", SLOT_target, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "currentTarget", SLOT_currentTarget, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "eventPhase", SLOT_eventPhase, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "bubbles", SLOT_bubbles, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "cancelable", SLOT_cancelable, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "timeStamp", SLOT_timeStamp, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "defaultPrevented", SLOT_defaultPrevented, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "isTrusted", SLOT_isTrusted, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec Event::sFunctions[] = {
  JS_FN("stopPropagation", StopPropagation, 0, FUNCTION_FLAGS),
  JS_FN("preventDefault", PreventDefault, 0, FUNCTION_FLAGS),
  JS_FN("initEvent", InitEvent, 3, FUNCTION_FLAGS),
  JS_FS_END
};

JSPropertySpec Event::sStaticProperties[] = {
  { "CAPTURING_PHASE", CAPTURING_PHASE, CONSTANT_FLAGS, GetConstant, NULL },
  { "AT_TARGET", AT_TARGET, CONSTANT_FLAGS, GetConstant, NULL },
  { "BUBBLING_PHASE", BUBBLING_PHASE, CONSTANT_FLAGS, GetConstant, NULL },
  { 0, 0, 0, NULL, NULL }
};

class MessageEvent : public Event
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

protected:
  uint64* mData;
  size_t mDataByteCount;
  nsTArray<nsCOMPtr<nsISupports> > mClonedObjects;
  bool mMainRuntime;

public:
  static bool
  IsThisClass(JSClass* aClass)
  {
    return aClass == &sClass || aClass == &sMainRuntimeClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto,
            bool aMainRuntime)
  {
    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    return JS_InitClass(aCx, aObj, aParentProto, clasp, Construct, 0,
                        sProperties, sFunctions, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, JSObject* aParent, JSAutoStructuredCloneBuffer& aData,
         nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects, bool aMainRuntime)
  {
    JSString* type = JS_InternString(aCx, "message");
    if (!type) {
      return NULL;
    }

    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    JSObject* obj = JS_NewObject(aCx, clasp, NULL, aParent);
    if (!obj) {
      return NULL;
    }

    MessageEvent* priv = new MessageEvent(aMainRuntime);
    if (!SetJSPrivateSafeish(aCx, obj, priv) ||
        !InitMessageEventCommon(aCx, obj, priv, type, false, false, NULL, NULL,
                                NULL, true)) {
      SetJSPrivateSafeish(aCx, obj, NULL);
      delete priv;
      return NULL;
    }

    aData.steal(&priv->mData, &priv->mDataByteCount);
    priv->mClonedObjects.SwapElements(aClonedObjects);

    return obj;
  }

protected:
  MessageEvent(bool aMainRuntime)
  : mData(NULL), mDataByteCount(0), mMainRuntime(aMainRuntime)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::MessageEvent);
  }

  virtual ~MessageEvent()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::MessageEvent);
    JS_ASSERT(!mData);
  }

  enum SLOT {
    SLOT_data = Event::SLOT_COUNT,
    SLOT_origin,
    SLOT_source,

    SLOT_COUNT,
    SLOT_FIRST = SLOT_data
  };

private:
  static MessageEvent*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    // JS_GetInstancePrivate is ok to be called with a null aObj, so this should
    // be too.
    JSClass* classPtr = NULL;

    if (aObj) {
      classPtr = JS_GET_CLASS(aCx, aObj);
      if (IsThisClass(classPtr)) {
        return GetJSPrivateSafeish<MessageEvent>(aCx, aObj);
      }
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr ? classPtr->name : "object");
    return NULL;
  }

  static JSBool
  InitMessageEventCommon(JSContext* aCx, JSObject* aObj, Event* aEvent,
                         JSString* aType, JSBool aBubbles, JSBool aCancelable,
                         JSString* aData, JSString* aOrigin, JSObject* aSource,
                         bool aIsTrusted)
  {
    jsval emptyString = JS_GetEmptyStringValue(aCx);

    if (!Event::InitEventCommon(aCx, aObj, aEvent, aType, aBubbles,
                                aCancelable, aIsTrusted) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_data,
                            aData ? STRING_TO_JSVAL(aData) : emptyString) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_origin,
                            aOrigin ? STRING_TO_JSVAL(aOrigin) : emptyString) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_source, OBJECT_TO_JSVAL(aSource))) {
      return false;
    }
    return true;
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
    JS_ASSERT(IsThisClass(JS_GET_CLASS(aCx, aObj)));
    MessageEvent* priv = GetJSPrivateSafeish<MessageEvent>(aCx, aObj);
    if (priv) {
      JS_free(aCx, priv->mData);
#ifdef DEBUG
      priv->mData = NULL;
#endif
      delete priv;
    }
  }

  static JSBool
  GetProperty(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32 slot = JSID_TO_INT(aIdval);

    JS_ASSERT(slot >= SLOT_data && slot < SLOT_COUNT);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    MessageEvent* event = GetInstancePrivate(aCx, aObj, name);
    if (!event) {
      return false;
    }

    // Deserialize and save the data value if we can.
    if (slot == SLOT_data && event->mData) {
      JSAutoStructuredCloneBuffer buffer;
      buffer.adopt(event->mData, event->mDataByteCount);

      event->mData = NULL;
      event->mDataByteCount = 0;

      // Release reference to objects that were AddRef'd for
      // cloning into worker when array goes out of scope.
      nsTArray<nsCOMPtr<nsISupports> > clonedObjects;
      clonedObjects.SwapElements(event->mClonedObjects);

      jsval data;
      if (!buffer.read(aCx, &data,
                       WorkerStructuredCloneCallbacks(event->mMainRuntime)) ||
          !JS_SetReservedSlot(aCx, aObj, slot, data)) {
        return false;
      }

      *aVp = data;
      return true;
    }

    return JS_GetReservedSlot(aCx, aObj, slot, aVp);
  }

  static JSBool
  InitMessageEvent(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    MessageEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JSString* type, *data, *origin;
    JSBool bubbles, cancelable;
    JSObject* source;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SbbSSo", &type,
                             &bubbles, &cancelable, &data, &origin, &source)) {
      return false;
    }

    return InitMessageEventCommon(aCx, obj, event, type, bubbles, cancelable,
                                  data, origin, source, false);
  }
};

#define DECL_MESSAGEEVENT_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, \
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT), \
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize, \
    JSCLASS_NO_OPTIONAL_MEMBERS \
  };

DECL_MESSAGEEVENT_CLASS(MessageEvent::sClass, "MessageEvent")
DECL_MESSAGEEVENT_CLASS(MessageEvent::sMainRuntimeClass, "WorkerMessageEvent")

#undef DECL_MESSAGEEVENT_CLASS

JSPropertySpec MessageEvent::sProperties[] = {
  { "data", SLOT_data, PROPERTY_FLAGS, GetProperty, js_GetterOnlyPropertyStub },
  { "origin", SLOT_origin, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "source", SLOT_source, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec MessageEvent::sFunctions[] = {
  JS_FN("initMessageEvent", InitMessageEvent, 6, FUNCTION_FLAGS),
  JS_FS_END
};

class ErrorEvent : public Event
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

public:
  static bool
  IsThisClass(JSClass* aClass)
  {
    return aClass == &sClass || aClass == &sMainRuntimeClass;
  }

  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto,
            bool aMainRuntime)
  {
    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    return JS_InitClass(aCx, aObj, aParentProto, clasp, Construct, 0,
                        sProperties, sFunctions, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, JSObject* aParent, JSString* aMessage,
         JSString* aFilename, uint32 aLineNumber, bool aMainRuntime)
  {
    JSString* type = JS_InternString(aCx, "error");
    if (!type) {
      return NULL;
    }

    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    JSObject* obj = JS_NewObject(aCx, clasp, NULL, aParent);
    if (!obj) {
      return NULL;
    }

    ErrorEvent* priv = new ErrorEvent();
    if (!SetJSPrivateSafeish(aCx, obj, priv) ||
        !InitErrorEventCommon(aCx, obj, priv, type, false, true, aMessage,
                              aFilename, aLineNumber, true)) {
      SetJSPrivateSafeish(aCx, obj, NULL);
      delete priv;
      return NULL;
    }
    return obj;
  }

protected:
  ErrorEvent()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::ErrorEvent);
  }

  virtual ~ErrorEvent()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::ErrorEvent);
  }

  enum SLOT {
    SLOT_message = Event::SLOT_COUNT,
    SLOT_filename,
    SLOT_lineno,

    SLOT_COUNT,
    SLOT_FIRST = SLOT_message
  };

private:
  static ErrorEvent*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    // JS_GetInstancePrivate is ok to be called with a null aObj, so this should
    // be too.
    JSClass* classPtr = NULL;

    if (aObj) {
      classPtr = JS_GET_CLASS(aCx, aObj);
      if (IsThisClass(classPtr)) {
        return GetJSPrivateSafeish<ErrorEvent>(aCx, aObj);
      }
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr ? classPtr->name : "object");
    return NULL;
  }

  static JSBool
  InitErrorEventCommon(JSContext* aCx, JSObject* aObj, Event* aEvent,
                       JSString* aType, JSBool aBubbles, JSBool aCancelable,
                       JSString* aMessage, JSString* aFilename,
                       uint32 aLineNumber, bool aIsTrusted)
  {
    if (!Event::InitEventCommon(aCx, aObj, aEvent, aType, aBubbles,
                                aCancelable, aIsTrusted) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_message,
                            STRING_TO_JSVAL(aMessage)) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_filename,
                            STRING_TO_JSVAL(aFilename)) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_lineno,
                            INT_TO_JSVAL(aLineNumber))) {
      return false;
    }
    return true;
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
    JS_ASSERT(IsThisClass(JS_GET_CLASS(aCx, aObj)));
    delete GetJSPrivateSafeish<ErrorEvent>(aCx, aObj);
  }

  static JSBool
  GetProperty(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32 slot = JSID_TO_INT(aIdval);

    JS_ASSERT(slot >= SLOT_message && slot < SLOT_COUNT);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    ErrorEvent* event = GetInstancePrivate(aCx, aObj, name);
    if (!event) {
      return false;
    }

    return JS_GetReservedSlot(aCx, aObj, slot, aVp);
  }

  static JSBool
  InitErrorEvent(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    ErrorEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JSString* type, *message, *filename;
    JSBool bubbles, cancelable;
    uint32 lineNumber;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SbbSSu", &type,
                             &bubbles, &cancelable, &message, &filename,
                             &lineNumber)) {
      return false;
    }

    return InitErrorEventCommon(aCx, obj, event, type, bubbles, cancelable,
                                message, filename, lineNumber, false);
  }
};

#define DECL_ERROREVENT_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, \
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT), \
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize, \
    JSCLASS_NO_OPTIONAL_MEMBERS \
  };

DECL_ERROREVENT_CLASS(ErrorEvent::sClass, "ErrorEvent")
DECL_ERROREVENT_CLASS(ErrorEvent::sMainRuntimeClass, "WorkerErrorEvent")

#undef DECL_ERROREVENT_CLASS

JSPropertySpec ErrorEvent::sProperties[] = {
  { "message", SLOT_message, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "filename", SLOT_filename, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "lineno", SLOT_lineno, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec ErrorEvent::sFunctions[] = {
  JS_FN("initErrorEvent", InitErrorEvent, 6, FUNCTION_FLAGS),
  JS_FS_END
};

class ProgressEvent : public Event
{
  static JSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

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
                        sProperties, sFunctions, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, JSObject* aParent, JSString* aType,
         bool aLengthComputable, jsdouble aLoaded, jsdouble aTotal)
  {
    JSString* type = JS_InternJSString(aCx, aType);
    if (!type) {
      return NULL;
    }

    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, aParent);
    if (!obj) {
      return NULL;
    }

    ProgressEvent* priv = new ProgressEvent();
    if (!SetJSPrivateSafeish(aCx, obj, priv) ||
        !InitProgressEventCommon(aCx, obj, priv, type, false, false,
                                 aLengthComputable, aLoaded, aTotal, true)) {
      SetJSPrivateSafeish(aCx, obj, NULL);
      delete priv;
      return NULL;
    }
    return obj;
  }

protected:
  ProgressEvent()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::ProgressEvent);
  }

  ~ProgressEvent()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::ProgressEvent);
  }

  enum SLOT {
    SLOT_lengthComputable = Event::SLOT_COUNT,
    SLOT_loaded,
    SLOT_total,

    SLOT_COUNT,
    SLOT_FIRST = SLOT_lengthComputable
  };

private:
  static ProgressEvent*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    JSClass* classPtr = NULL;

    if (aObj) {
      classPtr = JS_GET_CLASS(aCx, aObj);
      if (classPtr == &sClass) {
        return GetJSPrivateSafeish<ProgressEvent>(aCx, aObj);
      }
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr ? classPtr->name : "object");
    return NULL;
  }

  static JSBool
  InitProgressEventCommon(JSContext* aCx, JSObject* aObj, Event* aEvent,
                          JSString* aType, JSBool aBubbles, JSBool aCancelable,
                          JSBool aLengthComputable, jsdouble aLoaded,
                          jsdouble aTotal, bool aIsTrusted)
  {
    if (!Event::InitEventCommon(aCx, aObj, aEvent, aType, aBubbles,
                                aCancelable, aIsTrusted) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_lengthComputable,
                            aLengthComputable ? JSVAL_TRUE : JSVAL_FALSE) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_loaded, DOUBLE_TO_JSVAL(aLoaded)) ||
        !JS_SetReservedSlot(aCx, aObj, SLOT_total, DOUBLE_TO_JSVAL(aTotal))) {
      return false;
    }
    return true;
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
    delete GetJSPrivateSafeish<ProgressEvent>(aCx, aObj);
  }

  static JSBool
  GetProperty(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32 slot = JSID_TO_INT(aIdval);

    JS_ASSERT(slot >= SLOT_lengthComputable && slot < SLOT_COUNT);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    ProgressEvent* event = GetInstancePrivate(aCx, aObj, name);
    if (!event) {
      return false;
    }

    return JS_GetReservedSlot(aCx, aObj, slot, aVp);
  }

  static JSBool
  InitProgressEvent(JSContext* aCx, uintN aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);

    ProgressEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JSString* type;
    JSBool bubbles, cancelable, lengthComputable;
    jsdouble loaded, total;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "Sbbbdd", &type,
                             &bubbles, &cancelable, &lengthComputable, &loaded,
                             &total)) {
      return false;
    }

    return InitProgressEventCommon(aCx, obj, event, type, bubbles, cancelable,
                                   lengthComputable, loaded, total, false);
  }
};

JSClass ProgressEvent::sClass = {
  "ProgressEvent",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

JSPropertySpec ProgressEvent::sProperties[] = {
  { "lengthComputable", SLOT_lengthComputable, PROPERTY_FLAGS, GetProperty,
    js_GetterOnlyPropertyStub },
  { "loaded", SLOT_loaded, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { "total", SLOT_total, PROPERTY_FLAGS, GetProperty, 
    js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec ProgressEvent::sFunctions[] = {
  JS_FN("initProgressEvent", InitProgressEvent, 6, FUNCTION_FLAGS),
  JS_FS_END
};

Event*
Event::GetPrivate(JSContext* aCx, JSObject* aObj)
{
  if (aObj) {
    JSClass* classPtr = JS_GET_CLASS(aCx, aObj);
    if (IsThisClass(classPtr) ||
        MessageEvent::IsThisClass(classPtr) ||
        ErrorEvent::IsThisClass(classPtr) ||
        classPtr == ProgressEvent::Class()) {
      return GetJSPrivateSafeish<Event>(aCx, aObj);
    }
  }
  return NULL;
}

} /* anonymous namespace */

BEGIN_WORKERS_NAMESPACE

namespace events {

bool
InitClasses(JSContext* aCx, JSObject* aGlobal, bool aMainRuntime)
{
  JSObject* eventProto = Event::InitClass(aCx, aGlobal, aMainRuntime);
  if (!eventProto) {
    return false;
  }

  return MessageEvent::InitClass(aCx, aGlobal, eventProto, aMainRuntime) &&
         ErrorEvent::InitClass(aCx, aGlobal, eventProto, aMainRuntime) &&
         ProgressEvent::InitClass(aCx, aGlobal, eventProto);
}

JSObject*
CreateGenericEvent(JSContext* aCx, JSString* aType, bool aBubbles,
                   bool aCancelable, bool aMainRuntime)
{
  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  return Event::Create(aCx, global, aType, aBubbles, aCancelable, aMainRuntime);
}

JSObject*
CreateMessageEvent(JSContext* aCx, JSAutoStructuredCloneBuffer& aData,
                   nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
                   bool aMainRuntime)
{
  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  return MessageEvent::Create(aCx, global, aData, aClonedObjects, aMainRuntime);
}

JSObject*
CreateErrorEvent(JSContext* aCx, JSString* aMessage, JSString* aFilename,
                 uint32 aLineNumber, bool aMainRuntime)
{
  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  return ErrorEvent::Create(aCx, global, aMessage, aFilename, aLineNumber,
                            aMainRuntime);
}

JSObject*
CreateProgressEvent(JSContext* aCx, JSString* aType, bool aLengthComputable,
                    jsdouble aLoaded, jsdouble aTotal)
{
  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  return ProgressEvent::Create(aCx, global, aType, aLengthComputable, aLoaded,
                               aTotal);
}

bool
IsSupportedEventClass(JSContext* aCx, JSObject* aEvent)
{
  return Event::IsSupportedClass(aCx, aEvent);
}

bool
SetEventTarget(JSContext* aCx, JSObject* aEvent, JSObject* aTarget)
{
  return Event::SetTarget(aCx, aEvent, aTarget);
}

bool
EventWasCanceled(JSContext* aCx, JSObject* aEvent)
{
  return Event::WasCanceled(aCx, aEvent);
}

bool
DispatchEventToTarget(JSContext* aCx, JSObject* aTarget, JSObject* aEvent,
                      bool* aPreventDefaultCalled)
{
  jsval argv[] = { OBJECT_TO_JSVAL(aEvent) };
  jsval rval = JSVAL_VOID;
  if (!JS_CallFunctionName(aCx, aTarget, "dispatchEvent", JS_ARRAY_LENGTH(argv),
                           argv, &rval)) {
    return false;
  }

  JSBool preventDefaultCalled;
  if (!JS_ValueToBoolean(aCx, rval, &preventDefaultCalled)) {
    return false;
  }

  *aPreventDefaultCalled = !!preventDefaultCalled;
  return true;
}

} // namespace events

END_WORKERS_NAMESPACE
