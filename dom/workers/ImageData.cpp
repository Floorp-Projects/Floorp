/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageData.h"

#include "jsfriendapi.h"

#include "nsTraceRefcnt.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

USING_WORKERS_NAMESPACE

namespace {

class ImageData
{
  static JSClass sClass;
  static JSPropertySpec sProperties[];

  enum SLOT {
    SLOT_width = 0,
    SLOT_height,
    SLOT_data,

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
  Create(JSContext* aCx, uint32_t aWidth, uint32_t aHeight, JSObject *aData)
  {
    MOZ_ASSERT(aData);
    MOZ_ASSERT(JS_IsTypedArrayObject(aData));
    MOZ_ASSERT(JS_IsUint8ClampedArray(aData));

    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (!obj) {
      return NULL;
    }

    JS_SetReservedSlot(obj, SLOT_width, UINT_TO_JSVAL(aWidth));
    JS_SetReservedSlot(obj, SLOT_height, UINT_TO_JSVAL(aHeight));
    JS_SetReservedSlot(obj, SLOT_data, OBJECT_TO_JSVAL(aData));

    // This is an empty object. The point is just to differentiate instances
    // from the interface object.
    ImageData* priv = new ImageData();
    JS_SetPrivate(obj, priv);

    return obj;
  }

  static bool
  IsInstance(JSObject* aObj)
  {
    return JS_GetClass(aObj) == &sClass;
  }

  static uint32_t
  GetWidth(JSObject* aObj)
  {
    MOZ_ASSERT(IsInstance(aObj));
    return JS_DoubleToUint32(JS_GetReservedSlot(aObj, SLOT_width).toNumber());
  }

  static uint32_t
  GetHeight(JSObject* aObj)
  {
    MOZ_ASSERT(IsInstance(aObj));
    return JS_DoubleToUint32(JS_GetReservedSlot(aObj, SLOT_height).toNumber());
  }

  static
  JSObject* GetData(JSObject* aObj)
  {
    MOZ_ASSERT(IsInstance(aObj));
    return &JS_GetReservedSlot(aObj, SLOT_data).toObject();
  }

private:
  ImageData()
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::ImageData);
  }

  ~ImageData()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::ImageData);
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
    MOZ_ASSERT(JS_GetClass(aObj) == &sClass);
    delete static_cast<ImageData*>(JS_GetPrivate(aObj));
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

    MOZ_ASSERT(JSID_IS_INT(aIdval));
    MOZ_ASSERT(JSID_TO_INT(aIdval) >= 0 && JSID_TO_INT(aIdval) < SLOT_COUNT);

    aVp.set(JS_GetReservedSlot(aObj, JSID_TO_INT(aIdval)));
    return true;
  }
};

JSClass ImageData::sClass = {
  "ImageData",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

JSPropertySpec ImageData::sProperties[] = {
  // These properties are read-only per spec, which means that sets must throw
  // in strict mode and silently fail otherwise. This is a problem for workers
  // in general (because js_GetterOnlyPropertyStub throws unconditionally). The
  // general plan for fixing this involves the new DOM bindings. But Peace
  // Keeper breaks if we throw when setting these properties, so we need to do
  // something about it in the mean time. So we use NULL, which defaults to the
  // class setter (JS_StrictPropertyStub), which is always a silent no-op,
  // regardless of strict mode. Not ideal, but good enough for now.
  { "width", SLOT_width, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty), JSOP_NULLWRAPPER },
  { "height", SLOT_height, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty), JSOP_NULLWRAPPER },
  { "data", SLOT_data, PROPERTY_FLAGS, JSOP_WRAPPER(GetProperty), JSOP_NULLWRAPPER },
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace imagedata {

bool
InitClass(JSContext* aCx, JSObject* aGlobal)
{
  return !!ImageData::InitClass(aCx, aGlobal);
}

JSObject*
Create(JSContext* aCx, uint32_t aWidth, uint32_t aHeight, JSObject* aData)
{
  return ImageData::Create(aCx, aWidth, aHeight, aData);
}

bool
IsImageData(JSObject* aObj)
{
  return ImageData::IsInstance(aObj);
}

uint32_t
GetWidth(JSObject* aObj)
{
  return ImageData::GetWidth(aObj);
}

uint32_t
GetHeight(JSObject* aObj)
{
  return ImageData::GetHeight(aObj);
}

JSObject*
GetData(JSObject* aObj)
{
  return ImageData::GetData(aObj);
}


} // namespace imagedata

END_WORKERS_NAMESPACE
