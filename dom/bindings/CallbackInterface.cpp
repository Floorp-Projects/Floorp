/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CallbackInterface.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"  // JS::IsCallable
#include "js/CharacterEncoding.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty, JS_GetPropertyById
#include "mozilla/dom/BindingUtils.h"
#include "nsPrintfCString.h"

namespace mozilla::dom {

bool CallbackInterface::GetCallableProperty(
    BindingCallContext& cx, JS::Handle<jsid> aPropId,
    JS::MutableHandle<JS::Value> aCallable) {
  JS::Rooted<JSObject*> obj(cx, CallbackKnownNotGray());
  if (!JS_GetPropertyById(cx, obj, aPropId, aCallable)) {
    return false;
  }
  if (!aCallable.isObject() || !JS::IsCallable(&aCallable.toObject())) {
    JS::Rooted<JSString*> propId(cx, aPropId.toString());
    JS::UniqueChars propName = JS_EncodeStringToUTF8(cx, propId);
    nsPrintfCString description("Property '%s'", propName.get());
    cx.ThrowErrorMessage<MSG_NOT_CALLABLE>(description.get());
    return false;
  }

  return true;
}

}  // namespace mozilla::dom
