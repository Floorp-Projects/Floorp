/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "Events.h"
#include "mozilla/dom/BindingUtils.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsTraceRefcnt.h"

#include "WorkerInlines.h"
#include "WorkerPrivate.h"

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

using namespace mozilla;
USING_WORKERS_NAMESPACE

namespace {

class Event : public PrivatizableBase
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static const JSPropertySpec sProperties[];
  static const JSFunctionSpec sFunctions[];
  static const dom::ConstantSpec sStaticConstants[];

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
  InitClass(JSContext* aCx, JS::Handle<JSObject*> aObj, bool aMainRuntime)
  {
    JS::Rooted<JSObject*> parentProto(aCx);

    if (aMainRuntime) {
      JS::Rooted<JS::Value> windowPropVal(aCx);
      if (!JS_GetProperty(aCx, aObj, sClass.name, &windowPropVal)) {
        return NULL;
      }

      if (!JSVAL_IS_PRIMITIVE(windowPropVal)) {
        JS::Rooted<JS::Value> protoVal(aCx);
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

    JS::Rooted<JSObject*> proto(aCx, JS_InitClass(aCx, aObj, parentProto, clasp, Construct, 0,
                                                  sProperties, sFunctions, nullptr, nullptr));
    if (!proto) {
      return NULL;
    }

    JS::Rooted<JSObject*> ctor(aCx, JS_GetConstructor(aCx, proto));
    if (!ctor) {
      return NULL;
    }

    if (!dom::DefineConstants(aCx, ctor, sStaticConstants) ||
        !dom::DefineConstants(aCx, proto, sStaticConstants)) {
      return NULL;
    }

    return proto;
  }

  static JSObject*
  Create(JSContext* aCx, JS::Handle<JSObject*> aParent, JS::Handle<JSString*> aType,
         bool aBubbles, bool aCancelable, bool aMainRuntime)
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

  enum EventPhase {
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

  static bool
  IsEvent(const JS::Value& v)
  {
    return v.isObject() && GetPrivate(&v.toObject()) != nullptr;
  }

  template<SLOT Slot>
  static bool
  GetPropertyImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    aArgs.rval().set(JS_GetReservedSlot(&aArgs.thisv().toObject(), Slot));
    return true;
  }

  // This struct (versus just templating the method directly) is needed only for
  // gcc 4.4 (and maybe 4.5 -- 4.6 is okay) being too braindead to allow
  // GetProperty<Slot> and friends in the JSPropertySpec[] below.
  template<SLOT Slot>
  struct Property
  {
    static JSBool
    Get(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
    {
      static_assert(SLOT_FIRST <= Slot && Slot < SLOT_COUNT, "bad slot");
      JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
      return JS::CallNonGenericMethod<IsEvent, GetPropertyImpl<Slot> >(aCx, args);
    }
  };

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
    JS::Rooted<JSObject*> obj(aCx, JS_THIS_OBJECT(aCx, aVp));
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
    JS::Rooted<JSObject*> obj(aCx, JS_THIS_OBJECT(aCx, aVp));
    if (!obj) {
      return false;
    }

    Event* event = GetInstancePrivate(aCx, obj, sFunctions[2].name);
    if (!event) {
      return false;
    }

    JS::Rooted<JSString*> type(aCx);
    JSBool bubbles, cancelable;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "Sbb", type.address(),
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

DECL_EVENT_CLASS(Event::sClass, "WorkerEvent")
DECL_EVENT_CLASS(Event::sMainRuntimeClass, "WorkerEvent")

#undef DECL_EVENT_CLASS

const JSPropertySpec Event::sProperties[] = {
  JS_PSGS("type", Property<SLOT_type>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("target", Property<SLOT_target>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("currentTarget", Property<SLOT_currentTarget>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("eventPhase", Property<SLOT_eventPhase>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("bubbles", Property<SLOT_bubbles>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("cancelable", Property<SLOT_cancelable>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("timeStamp", Property<SLOT_timeStamp>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("defaultPrevented", Property<SLOT_defaultPrevented>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("isTrusted", Property<SLOT_isTrusted>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PS_END
};

const JSFunctionSpec Event::sFunctions[] = {
  JS_FN("stopPropagation", StopPropagation, 0, FUNCTION_FLAGS),
  JS_FN("preventDefault", PreventDefault, 0, FUNCTION_FLAGS),
  JS_FN("initEvent", InitEvent, 3, FUNCTION_FLAGS),
  JS_FN("stopImmediatePropagation", StopImmediatePropagation, 0, FUNCTION_FLAGS),
  JS_FS_END
};

const dom::ConstantSpec Event::sStaticConstants[] = {
  { "CAPTURING_PHASE", JS::Int32Value(CAPTURING_PHASE) },
  { "AT_TARGET", JS::Int32Value(AT_TARGET) },
  { "BUBBLING_PHASE", JS::Int32Value(BUBBLING_PHASE) },
  { nullptr, JS::UndefinedValue() }
};

class MessageEvent : public Event
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static const JSPropertySpec sProperties[];
  static const JSFunctionSpec sFunctions[];

protected:
  JSAutoStructuredCloneBuffer mBuffer;
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
  Create(JSContext* aCx, JS::Handle<JSObject*> aParent, JSAutoStructuredCloneBuffer& aData,
         nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects, bool aMainRuntime)
  {
    JS::Rooted<JSString*> type(aCx, JS_InternString(aCx, "message"));
    if (!type) {
      return NULL;
    }

    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    JS::Rooted<JSObject*> obj(aCx, JS_NewObject(aCx, clasp, NULL, aParent));
    if (!obj) {
      return NULL;
    }

    MessageEvent* priv = new MessageEvent(aMainRuntime);
    SetJSPrivateSafeish(obj, priv);
    InitMessageEventCommon(aCx, obj, priv, type, false, false, NULL, NULL, NULL,
                           true);
    priv->mBuffer.swap(aData);
    priv->mClonedObjects.SwapElements(aClonedObjects);

    return obj;
  }

protected:
  MessageEvent(bool aMainRuntime)
  : mMainRuntime(aMainRuntime)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::MessageEvent);
  }

  virtual ~MessageEvent()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::MessageEvent);
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
    delete priv;
  }

  static bool
  IsMessageEvent(const JS::Value& v)
  {
    if (!v.isObject())
      return false;
    JSObject* obj = &v.toObject();
    return IsThisClass(JS_GetClass(obj)) &&
           GetJSPrivateSafeish<MessageEvent>(obj) != nullptr;
  }

  template<SLOT Slot>
  static bool
  GetPropertyImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    JS::Rooted<JSObject*> obj(aCx, &aArgs.thisv().toObject());

    const char* name = sProperties[Slot - SLOT_FIRST].name;
    MessageEvent* event = GetInstancePrivate(aCx, obj, name);
    MOZ_ASSERT(event);

    // Deserialize and save the data value if we can.
    if (Slot == SLOT_data && event->mBuffer.data()) {
      JSAutoStructuredCloneBuffer buffer;
      buffer.swap(event->mBuffer);

      // Release reference to objects that were AddRef'd for
      // cloning into worker when array goes out of scope.
      nsTArray<nsCOMPtr<nsISupports> > clonedObjects;
      clonedObjects.SwapElements(event->mClonedObjects);

      JS::Rooted<JS::Value> data(aCx);
      if (!buffer.read(aCx, data.address(),
                       WorkerStructuredCloneCallbacks(event->mMainRuntime))) {
        return false;
      }
      JS_SetReservedSlot(obj, Slot, data);

      aArgs.rval().set(data);
      return true;
    }

    aArgs.rval().set(JS_GetReservedSlot(obj, Slot));
    return true;
  }

  // This struct (versus just templating the method directly) is needed only for
  // gcc 4.4 (and maybe 4.5 -- 4.6 is okay) being too braindead to allow
  // GetProperty<Slot> and friends in the JSPropertySpec[] below.
  template<SLOT Slot>
  struct Property
  {
    static JSBool
    Get(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
    {
      static_assert(SLOT_FIRST <= Slot && Slot < SLOT_COUNT, "bad slot");
      JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
      return JS::CallNonGenericMethod<IsMessageEvent, GetPropertyImpl<Slot> >(aCx, args);
    }
  };

  static JSBool
  InitMessageEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS::Rooted<JSObject*> obj(aCx, JS_THIS_OBJECT(aCx, aVp));
    if (!obj) {
      return false;
    }

    MessageEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JS::Rooted<JSString*> type(aCx), data(aCx), origin(aCx);
    JSBool bubbles, cancelable;
    JS::Rooted<JSObject*> source(aCx);
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SbbSSo", type.address(),
                             &bubbles, &cancelable, data.address(),
                             origin.address(), source.address())) {
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

DECL_MESSAGEEVENT_CLASS(MessageEvent::sClass, "WorkerMessageEvent")
DECL_MESSAGEEVENT_CLASS(MessageEvent::sMainRuntimeClass, "WorkerMessageEvent")

#undef DECL_MESSAGEEVENT_CLASS

const JSPropertySpec MessageEvent::sProperties[] = {
  JS_PSGS("data", Property<SLOT_data>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("origin", Property<SLOT_origin>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("source", Property<SLOT_source>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PS_END
};

const JSFunctionSpec MessageEvent::sFunctions[] = {
  JS_FN("initMessageEvent", InitMessageEvent, 6, FUNCTION_FLAGS),
  JS_FS_END
};

class ErrorEvent : public Event
{
  static JSClass sClass;
  static JSClass sMainRuntimeClass;

  static const JSPropertySpec sProperties[];
  static const JSFunctionSpec sFunctions[];

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
  Create(JSContext* aCx, JS::Handle<JSObject*> aParent, JS::Handle<JSString*> aMessage,
         JS::Handle<JSString*> aFilename, uint32_t aLineNumber, bool aMainRuntime)
  {
    JS::Rooted<JSString*> type(aCx, JS_InternString(aCx, "error"));
    if (!type) {
      return NULL;
    }

    JSClass* clasp = aMainRuntime ? &sMainRuntimeClass : &sClass;

    JS::Rooted<JSObject*> obj(aCx, JS_NewObject(aCx, clasp, NULL, aParent));
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

  static bool
  IsErrorEvent(const JS::Value& v)
  {
    if (!v.isObject())
      return false;
    JSObject* obj = &v.toObject();
    return IsThisClass(JS_GetClass(obj)) &&
           GetJSPrivateSafeish<ErrorEvent>(obj) != nullptr;
  }

  template<SLOT Slot>
  static bool
  GetPropertyImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    aArgs.rval().set(JS_GetReservedSlot(&aArgs.thisv().toObject(), Slot));
    return true;
  }

  // This struct (versus just templating the method directly) is needed only for
  // gcc 4.4 (and maybe 4.5 -- 4.6 is okay) being too braindead to allow
  // GetProperty<Slot> and friends in the JSPropertySpec[] below.
  template<SLOT Slot>
  struct Property
  {
    static JSBool
    Get(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
    {
      static_assert(SLOT_FIRST <= Slot && Slot < SLOT_COUNT, "bad slot");
      JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
      return JS::CallNonGenericMethod<IsErrorEvent, GetPropertyImpl<Slot> >(aCx, args);
    }
  };

  static JSBool
  InitErrorEvent(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS::Rooted<JSObject*> obj(aCx, JS_THIS_OBJECT(aCx, aVp));
    if (!obj) {
      return false;
    }

    ErrorEvent* event = GetInstancePrivate(aCx, obj, sFunctions[0].name);
    if (!event) {
      return false;
    }

    JS::Rooted<JSString*> type(aCx), message(aCx), filename(aCx);
    JSBool bubbles, cancelable;
    uint32_t lineNumber;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "SbbSSu", type.address(),
                             &bubbles, &cancelable, message.address(),
                             filename.address(), &lineNumber)) {
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

DECL_ERROREVENT_CLASS(ErrorEvent::sClass, "WorkerErrorEvent")
DECL_ERROREVENT_CLASS(ErrorEvent::sMainRuntimeClass, "WorkerErrorEvent")

#undef DECL_ERROREVENT_CLASS

const JSPropertySpec ErrorEvent::sProperties[] = {
  JS_PSGS("message", Property<SLOT_message>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("filename", Property<SLOT_filename>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("lineno", Property<SLOT_lineno>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PS_END
};

const JSFunctionSpec ErrorEvent::sFunctions[] = {
  JS_FN("initErrorEvent", InitErrorEvent, 6, FUNCTION_FLAGS),
  JS_FS_END
};

class ProgressEvent : public Event
{
  static JSClass sClass;
  static const JSPropertySpec sProperties[];

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
  Create(JSContext* aCx, JS::Handle<JSObject*> aParent, JSString* aType,
         bool aLengthComputable, double aLoaded, double aTotal)
  {
    JS::Rooted<JSString*> type(aCx, JS_InternJSString(aCx, aType));
    if (!type) {
      return NULL;
    }

    JS::Rooted<JSObject*> obj(aCx, JS_NewObject(aCx, &sClass, NULL, aParent));
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

  static bool
  IsProgressEvent(const JS::Value& v)
  {
    if (!v.isObject())
      return false;
    JSObject* obj = &v.toObject();
    return JS_GetClass(obj) == &sClass &&
           GetJSPrivateSafeish<ProgressEvent>(obj) != nullptr;
  }

  template<SLOT Slot>
  static bool
  GetPropertyImpl(JSContext* aCx, JS::CallArgs aArgs)
  {
    aArgs.rval().set(JS_GetReservedSlot(&aArgs.thisv().toObject(), Slot));
    return true;
  }

  // This struct (versus just templating the method directly) is needed only for
  // gcc 4.4 (and maybe 4.5 -- 4.6 is okay) being too braindead to allow
  // GetProperty<Slot> and friends in the JSPropertySpec[] below.
  template<SLOT Slot>
  struct Property
  {
    static JSBool
    Get(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
    {
      static_assert(SLOT_FIRST <= Slot && Slot < SLOT_COUNT, "bad slot");
      JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);
      return JS::CallNonGenericMethod<IsProgressEvent, GetPropertyImpl<Slot> >(aCx, args);
    }
  };
};

JSClass ProgressEvent::sClass = {
  "WorkerProgressEvent",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

const JSPropertySpec ProgressEvent::sProperties[] = {
  JS_PSGS("lengthComputable", Property<SLOT_lengthComputable>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("loaded", Property<SLOT_loaded>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PSGS("total", Property<SLOT_total>::Get, GetterOnlyJSNative,
          JSPROP_ENUMERATE),
  JS_PS_END
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
InitClasses(JSContext* aCx, JS::Handle<JSObject*> aGlobal, bool aMainRuntime)
{
  JS::Rooted<JSObject*> eventProto(aCx, Event::InitClass(aCx, aGlobal, aMainRuntime));
  if (!eventProto) {
    return false;
  }

  return MessageEvent::InitClass(aCx, aGlobal, eventProto, aMainRuntime) &&
         ErrorEvent::InitClass(aCx, aGlobal, eventProto, aMainRuntime) &&
         ProgressEvent::InitClass(aCx, aGlobal, eventProto);
}

JSObject*
CreateGenericEvent(JSContext* aCx, JS::Handle<JSString*> aType, bool aBubbles,
                   bool aCancelable, bool aMainRuntime)
{
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  return Event::Create(aCx, global, aType, aBubbles, aCancelable, aMainRuntime);
}

JSObject*
CreateMessageEvent(JSContext* aCx, JSAutoStructuredCloneBuffer& aData,
                   nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
                   bool aMainRuntime)
{
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  return MessageEvent::Create(aCx, global, aData, aClonedObjects, aMainRuntime);
}

JSObject*
CreateErrorEvent(JSContext* aCx, JS::Handle<JSString*> aMessage,
                 JS::Handle<JSString*> aFilename, uint32_t aLineNumber,
                 bool aMainRuntime)
{
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  return ErrorEvent::Create(aCx, global, aMessage, aFilename, aLineNumber,
                            aMainRuntime);
}

JSObject*
CreateProgressEvent(JSContext* aCx, JSString* aType, bool aLengthComputable,
                    double aLoaded, double aTotal)
{
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
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
DispatchEventToTarget(JSContext* aCx, JS::Handle<JSObject*> aTarget,
                      JS::Handle<JSObject*> aEvent, bool* aPreventDefaultCalled)
{
  static const char kFunctionName[] = "dispatchEvent";
  JSBool hasProperty;
  if (!JS_HasProperty(aCx, aTarget, kFunctionName, &hasProperty)) {
    return false;
  }

  JSBool preventDefaultCalled = false;
  if (hasProperty) {
    jsval argv[] = { OBJECT_TO_JSVAL(aEvent) };
    JS::Rooted<JS::Value> rval(aCx, JS::UndefinedValue());
    if (!JS_CallFunctionName(aCx, aTarget, kFunctionName, ArrayLength(argv),
                             argv, rval.address()) ||
        !JS_ValueToBoolean(aCx, rval, &preventDefaultCalled)) {
      return false;
    }
  }

  *aPreventDefaultCalled = !!preventDefaultCalled;
  return true;
}

} // namespace events

END_WORKERS_NAMESPACE
