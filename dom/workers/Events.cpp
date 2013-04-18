/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "Events.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsTraceRefcnt.h"

#include "WorkerInlines.h"
#include "WorkerPrivate.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

#define CONSTANT_FLAGS \
  JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_PERMANENT | JSPROP_READONLY

using namespace mozilla;
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
  bool mStopImmediatePropagationCalled;

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
      SetJSPrivateSafeish(obj, priv);
      InitEventCommon(obj, priv, aType, aBubbles, aCancelable, true);
    }
    return obj;
  }

  static bool
  IsSupportedClass(JSObject* aEvent)
  {
    return !!GetPrivate(aEvent);
  }

  static void
  SetTarget(JSObject* aEvent, JSObject* aTarget)
  {
    JS_ASSERT(IsSupportedClass(aEvent));

    jsval target = OBJECT_TO_JSVAL(aTarget);
    JS_SetReservedSlot(aEvent, SLOT_target, target);
    JS_SetReservedSlot(aEvent, SLOT_currentTarget, target);
  }

  static bool
  WasCanceled(JSObject* aEvent)
  {
    JS_ASSERT(IsSupportedClass(aEvent));

    jsval canceled = JS_GetReservedSlot(aEvent, SLOT_defaultPrevented);
    return JSVAL_TO_BOOLEAN(canceled);
  }

  static bool
  ImmediatePropagationStopped(JSObject* aEvent)
  {
    Event* event = GetPrivate(aEvent);
    return event ? event->mStopImmediatePropagationCalled : false;
  }

protected:
  Event()
  : mStopPropagationCalled(false), mStopImmediatePropagationCalled(false)
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
  GetPrivate(JSObject* aEvent);

  static Event*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    Event* priv = GetPrivate(aObj);
    if (priv) {
      return priv;
    }
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         JS_GetClass(aObj)->name);
    return NULL;
  }

  static void
  InitEventCommon(JSObject* aObj, Event* aEvent, JSString* aType,
                  JSBool aBubbles, JSBool aCancelable, bool aIsTrusted)
  {
    aEvent->mStopPropagationCalled = false;
    aEvent->mStopImmediatePropagationCalled = false;

    JS_SetReservedSlot(aObj, SLOT_type, STRING_TO_JSVAL(aType));
    JS_SetReservedSlot(aObj, SLOT_target, JSVAL_NULL);
    JS_SetReservedSlot(aObj, SLOT_currentTarget, JSVAL_NULL);
    JS_SetReservedSlot(aObj, SLOT_eventPhase, INT_TO_JSVAL(CAPTURING_PHASE));
    JS_SetReservedSlot(aObj, SLOT_bubbles,
                       aBubbles ? JSVAL_TRUE : JSVAL_FALSE);
    JS_SetReservedSlot(aObj, SLOT_cancelable,
                       aCancelable ? JSVAL_TRUE : JSVAL_FALSE);
    JS_SetReservedSlot(aObj, SLOT_timeStamp, JS::NumberValue(double(JS_Now())));
    JS_SetReservedSlot(aObj, SLOT_defaultPrevented, JSVAL_FALSE);
    JS_SetReservedSlot(aObj, SLOT_isTrusted,
                       aIsTrusted ? JSVAL_TRUE : JSVAL_FALSE);
  }

private:
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
    JS_ASSERT(IsThisClass(JS_GetClass(aObj)));
    delete GetJSPrivateSafeish<Event>(aObj);
  }

  static JSBool
  GetProperty(JSContext* aCx, JSHandleObject aObj, JSHandleId aIdval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32_t slot = JSID_TO_INT(aIdval);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    if (!GetInstancePrivate(aCx, aObj, name)) {
      return false;
    }

    aVp.set(JS_GetReservedSlot(aObj, slot));
    return true;
  }

  static JSBool
  GetConstant(JSContext* aCx, JSHandleObject aObj, JSHandleId idval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(idval));
    JS_ASSERT(JSID_TO_INT(idval) >= CAPTURING_PHASE &&
              JSID_TO_INT(idval) <= BUBBLING_PHASE);

    aVp.set(INT_TO_JSVAL(JSID_TO_INT(idval)));
    return true;
  }

  static JSBool
  StopPropagation(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    event->mStopPropagationCalled = true;

    return true;
  }

  static JSBool
  StopImmediatePropagation(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[3].name);
    if (!event) {
      return false;
    }

    event->mStopImmediatePropagationCalled = true;

    return true;
  }
  
  static JSBool
  PreventDefault(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[1].name);
    if (!event) {
      return false;
    }

    jsval cancelableVal = JS_GetReservedSlot(obj, SLOT_cancelable);

    if (JSVAL_TO_BOOLEAN(cancelableVal))
      JS_SetReservedSlot(obj, SLOT_defaultPrevented, cancelableVal);
    return true;
  }

  static JSBool
  InitEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

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

    InitEventCommon(obj, event, type, bubbles, cancelable, false);
    return true;
  }
};

#define DECL_EVENT_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, \
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT), \
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize \
  };

DECL_EVENT_CLASS(Event::sClass, "Event")
DECL_EVENT_CLASS(Event::sMainRuntimeClass, "WorkerEvent")

#undef DECL_EVENT_CLASS

JSPropertySpec Event::sProperties[] = {
  { "type", SLOT_type, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "target", SLOT_target, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "currentTarget", SLOT_currentTarget, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "eventPhase", SLOT_eventPhase, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "bubbles", SLOT_bubbles, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "cancelable", SLOT_cancelable, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "timeStamp", SLOT_timeStamp, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "defaultPrevented", SLOT_defaultPrevented, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "isTrusted", SLOT_isTrusted, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

JSFunctionSpec Event::sFunctions[] = {
  JS_FN("stopPropagation", StopPropagation, 0, FUNCTION_FLAGS),
  JS_FN("preventDefault", PreventDefault, 0, FUNCTION_FLAGS),
  JS_FN("initEvent", InitEvent, 3, FUNCTION_FLAGS),
  JS_FN("stopImmediatePropagation", StopImmediatePropagation, 0, FUNCTION_FLAGS),
  JS_FS_END
};

JSPropertySpec Event::sStaticProperties[] = {
  { "CAPTURING_PHASE", CAPTURING_PHASE, CONSTANT_FLAGS,
    JSOP_WRAPPER(GetConstant), JSOP_NULLWRAPPER },
  { "AT_TARGET", AT_TARGET, CONSTANT_FLAGS, JSOP_WRAPPER(GetConstant), JSOP_NULLWRAPPER },
  { "BUBBLING_PHASE", BUBBLING_PHASE, CONSTANT_FLAGS,
    JSOP_WRAPPER(GetConstant), JSOP_NULLWRAPPER },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

class MessageEvent : public Event
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

protected:
  uint64_t* mData;
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
    SetJSPrivateSafeish(obj, priv);
    InitMessageEventCommon(aCx, obj, priv, type, false, false, NULL, NULL, NULL,
                           true);
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
    JSClass* classPtr = JS_GetClass(aObj);
    if (IsThisClass(classPtr)) {
      return GetJSPrivateSafeish<MessageEvent>(aObj);
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr->name);
    return NULL;
  }

  static void
  InitMessageEventCommon(JSContext* aCx, JSObject* aObj, Event* aEvent,
                         JSString* aType, JSBool aBubbles, JSBool aCancelable,
                         JSString* aData, JSString* aOrigin, JSObject* aSource,
                         bool aIsTrusted)
  {
    jsval emptyString = JS_GetEmptyStringValue(aCx);

    Event::InitEventCommon(aObj, aEvent, aType, aBubbles, aCancelable,
                           aIsTrusted);
    JS_SetReservedSlot(aObj, SLOT_data,
                       aData ? STRING_TO_JSVAL(aData) : emptyString);
    JS_SetReservedSlot(aObj, SLOT_origin,
                       aOrigin ? STRING_TO_JSVAL(aOrigin) : emptyString);
    JS_SetReservedSlot(aObj, SLOT_source, OBJECT_TO_JSVAL(aSource));
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
    JS_ASSERT(IsThisClass(JS_GetClass(aObj)));
    MessageEvent* priv = GetJSPrivateSafeish<MessageEvent>(aObj);
    if (priv) {
      JS_freeop(aFop, priv->mData);
#ifdef DEBUG
      priv->mData = NULL;
#endif
      delete priv;
    }
  }

  static JSBool
  GetProperty(JSContext* aCx, JSHandleObject aObj, JSHandleId aIdval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32_t slot = JSID_TO_INT(aIdval);

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
                       WorkerStructuredCloneCallbacks(event->mMainRuntime))) {
        return false;
      }
      JS_SetReservedSlot(aObj, slot, data);

      aVp.set(data);
      return true;
    }

    aVp.set(JS_GetReservedSlot(aObj, slot));
    return true;
  }

  static JSBool
  InitMessageEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

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

    InitMessageEventCommon(aCx, obj, event, type, bubbles, cancelable,
                           data, origin, source, false);
    return true;
  }
};

#define DECL_MESSAGEEVENT_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, \
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT), \
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize \
  };

DECL_MESSAGEEVENT_CLASS(MessageEvent::sClass, "MessageEvent")
DECL_MESSAGEEVENT_CLASS(MessageEvent::sMainRuntimeClass, "WorkerMessageEvent")

#undef DECL_MESSAGEEVENT_CLASS

JSPropertySpec MessageEvent::sProperties[] = {
  { "data", SLOT_data, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "origin", SLOT_origin, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "source", SLOT_source, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
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
         JSString* aFilename, uint32_t aLineNumber, bool aMainRuntime)
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
    SetJSPrivateSafeish(obj, priv);
    InitErrorEventCommon(obj, priv, type, false, true, aMessage, aFilename,
                         aLineNumber, true);
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
    JSClass* classPtr = JS_GetClass(aObj);
    if (IsThisClass(classPtr)) {
      return GetJSPrivateSafeish<ErrorEvent>(aObj);
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr->name);
    return NULL;
  }

  static void
  InitErrorEventCommon(JSObject* aObj, Event* aEvent, JSString* aType,
                       JSBool aBubbles, JSBool aCancelable,
                       JSString* aMessage, JSString* aFilename,
                       uint32_t aLineNumber, bool aIsTrusted)
  {
    Event::InitEventCommon(aObj, aEvent, aType, aBubbles, aCancelable,
                           aIsTrusted);
    JS_SetReservedSlot(aObj, SLOT_message, STRING_TO_JSVAL(aMessage));
    JS_SetReservedSlot(aObj, SLOT_filename, STRING_TO_JSVAL(aFilename));
    JS_SetReservedSlot(aObj, SLOT_lineno, INT_TO_JSVAL(aLineNumber));
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
    JS_ASSERT(IsThisClass(JS_GetClass(aObj)));
    delete GetJSPrivateSafeish<ErrorEvent>(aObj);
  }

  static JSBool
  GetProperty(JSContext* aCx, JSHandleObject aObj, JSHandleId aIdval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32_t slot = JSID_TO_INT(aIdval);

    JS_ASSERT(slot >= SLOT_message && slot < SLOT_COUNT);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    ErrorEvent* event = GetInstancePrivate(aCx, aObj, name);
    if (!event) {
      return false;
    }

    aVp.set(JS_GetReservedSlot(aObj, slot));
    return true;
  }

  static JSBool
  InitErrorEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    ErrorEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JSString* type, *message, *filename;
    JSBool bubbles, cancelable;
    uint32_t lineNumber;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SbbSSu", &type,
                             &bubbles, &cancelable, &message, &filename,
                             &lineNumber)) {
      return false;
    }

    InitErrorEventCommon(obj, event, type, bubbles, cancelable, message,
                         filename, lineNumber, false);
    return true;
  }
};

#define DECL_ERROREVENT_CLASS(_varname, _name) \
  JSClass _varname = { \
    _name, \
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT), \
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, \
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize \
  };

DECL_ERROREVENT_CLASS(ErrorEvent::sClass, "ErrorEvent")
DECL_ERROREVENT_CLASS(ErrorEvent::sMainRuntimeClass, "WorkerErrorEvent")

#undef DECL_ERROREVENT_CLASS

JSPropertySpec ErrorEvent::sProperties[] = {
  { "message", SLOT_message, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "filename", SLOT_filename, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "lineno", SLOT_lineno, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
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
         bool aLengthComputable, double aLoaded, double aTotal)
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
    SetJSPrivateSafeish(obj, priv);
    InitProgressEventCommon(obj, priv, type, false, false, aLengthComputable,
                            aLoaded, aTotal, true);
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
    JSClass* classPtr = JS_GetClass(aObj);
    if (classPtr == &sClass) {
      return GetJSPrivateSafeish<ProgressEvent>(aObj);
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         classPtr->name);
    return NULL;
  }

  static void
  InitProgressEventCommon(JSObject* aObj, Event* aEvent, JSString* aType,
                          JSBool aBubbles, JSBool aCancelable,
                          JSBool aLengthComputable, double aLoaded,
                          double aTotal, bool aIsTrusted)
  {
    Event::InitEventCommon(aObj, aEvent, aType, aBubbles, aCancelable,
                           aIsTrusted);
    JS_SetReservedSlot(aObj, SLOT_lengthComputable,
                       aLengthComputable ? JSVAL_TRUE : JSVAL_FALSE);
    JS_SetReservedSlot(aObj, SLOT_loaded, DOUBLE_TO_JSVAL(aLoaded));
    JS_SetReservedSlot(aObj, SLOT_total, DOUBLE_TO_JSVAL(aTotal));
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
    delete GetJSPrivateSafeish<ProgressEvent>(aObj);
  }

  static JSBool
  GetProperty(JSContext* aCx, JSHandleObject aObj, JSHandleId aIdval, JSMutableHandleValue aVp)
  {
    JS_ASSERT(JSID_IS_INT(aIdval));

    int32_t slot = JSID_TO_INT(aIdval);

    JS_ASSERT(slot >= SLOT_lengthComputable && slot < SLOT_COUNT);

    const char*& name = sProperties[slot - SLOT_FIRST].name;
    ProgressEvent* event = GetInstancePrivate(aCx, aObj, name);
    if (!event) {
      return false;
    }

    aVp.set(JS_GetReservedSlot(aObj, slot));
    return true;
  }

  static JSBool
  InitProgressEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    ProgressEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JSString* type;
    JSBool bubbles, cancelable, lengthComputable;
    double loaded, total;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "Sbbbdd", &type,
                             &bubbles, &cancelable, &lengthComputable, &loaded,
                             &total)) {
      return false;
    }

    InitProgressEventCommon(obj, event, type, bubbles, cancelable,
                            lengthComputable, loaded, total, false);
    return true;
  }
};

JSClass ProgressEvent::sClass = {
  "ProgressEvent",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

JSPropertySpec ProgressEvent::sProperties[] = {
  { "lengthComputable", SLOT_lengthComputable, PROPERTY_FLAGS,
    JSOP_WRAPPER(GetProperty), JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "loaded", SLOT_loaded, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { "total", SLOT_total, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty),
    JSOP_WRAPPER(js_GetterOnlyPropertyStub) },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

JSFunctionSpec ProgressEvent::sFunctions[] = {
  JS_FN("initProgressEvent", InitProgressEvent, 6, FUNCTION_FLAGS),
  JS_FS_END
};

Event*
Event::GetPrivate(JSObject* aObj)
{
  if (aObj) {
    JSClass* classPtr = JS_GetClass(aObj);
    if (IsThisClass(classPtr) ||
        MessageEvent::IsThisClass(classPtr) ||
        ErrorEvent::IsThisClass(classPtr) ||
        classPtr == ProgressEvent::Class()) {
      return GetJSPrivateSafeish<Event>(aObj);
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
                 uint32_t aLineNumber, bool aMainRuntime)
{
  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  return ErrorEvent::Create(aCx, global, aMessage, aFilename, aLineNumber,
                            aMainRuntime);
}

JSObject*
CreateProgressEvent(JSContext* aCx, JSString* aType, bool aLengthComputable,
                    double aLoaded, double aTotal)
{
  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  return ProgressEvent::Create(aCx, global, aType, aLengthComputable, aLoaded,
                               aTotal);
}

bool
IsSupportedEventClass(JSObject* aEvent)
{
  return Event::IsSupportedClass(aEvent);
}

void
SetEventTarget(JSObject* aEvent, JSObject* aTarget)
{
  Event::SetTarget(aEvent, aTarget);
}

bool
EventWasCanceled(JSObject* aEvent)
{
  return Event::WasCanceled(aEvent);
}

bool
EventImmediatePropagationStopped(JSObject* aEvent)
{
  return Event::ImmediatePropagationStopped(aEvent);
}

bool
DispatchEventToTarget(JSContext* aCx, JSObject* aTarget, JSObject* aEvent,
                      bool* aPreventDefaultCalled)
{
  static const char kFunctionName[] = "dispatchEvent";
  JSBool hasProperty;
  if (!JS_HasProperty(aCx, aTarget, kFunctionName, &hasProperty)) {
    return false;
  }

  JSBool preventDefaultCalled = false;
  if (hasProperty) {
    jsval argv[] = { OBJECT_TO_JSVAL(aEvent) };
    jsval rval = JSVAL_VOID;
    if (!JS_CallFunctionName(aCx, aTarget, kFunctionName, ArrayLength(argv),
                             argv, &rval) ||
        !JS_ValueToBoolean(aCx, rval, &preventDefaultCalled)) {
      return false;
    }
  }

  *aPreventDefaultCalled = !!preventDefaultCalled;
  return true;
}

} // namespace events

END_WORKERS_NAMESPACE
