/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomQS_Canvas_h
#define CustomQS_Canvas_h

#include "jsapi.h"

static bool
GetPositiveInt(JSContext* cx, JSObject& obj, const char* name, uint32_t* out)
{
  JS::Value temp;
  int32_t signedInt;
  if (!JS_GetProperty(cx, &obj, name, &temp) ||
      !JS_ValueToECMAInt32(cx, temp, &signedInt)) {
    return false;
  }
  if (signedInt <= 0) {
    return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);
  }
  *out = uint32_t(signedInt);
  return true;
}

static bool
GetImageData(JSContext* cx, const JS::Value& imageData,
             uint32_t* width, uint32_t* height, JS::Anchor<JSObject*>* array)
{
  if (!imageData.isObject()) {
    return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);
  }

  JSObject& dataObject = imageData.toObject();

  if (!GetPositiveInt(cx, dataObject, "width", width) ||
      !GetPositiveInt(cx, dataObject, "height", height)) {
    return false;
  }

  JS::Value temp;
  if (!JS_GetProperty(cx, &dataObject, "data", &temp)) {
    return false;
  }
  if (!temp.isObject()) {
    return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);
  }
  array->set(&temp.toObject());
  return true;
}

#endif // CustomQS_Canvas_h
