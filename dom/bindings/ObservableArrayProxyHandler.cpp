/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ObservableArrayProxyHandler.h"

#include "jsapi.h"
#include "js/friend/ErrorMessages.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "mozilla/dom/JSSlots.h"
#include "mozilla/dom/ProxyHandlerUtils.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/ErrorResult.h"
#include "nsDebug.h"
#include "nsJSUtils.h"

namespace mozilla::dom {

const char ObservableArrayProxyHandler::family = 0;

bool ObservableArrayProxyHandler::defineProperty(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::Handle<JS::PropertyKey> aId, JS::Handle<JS::PropertyDescriptor> aDesc,
    JS::ObjectOpResult& aResult) const {
  if (aId.get() == s_length_id) {
    if (aDesc.isAccessorDescriptor()) {
      return aResult.failNotDataDescriptor();
    }
    if (aDesc.hasConfigurable() && aDesc.configurable()) {
      return aResult.failInvalidDescriptor();
    }
    if (aDesc.hasEnumerable() && aDesc.enumerable()) {
      return aResult.failInvalidDescriptor();
    }
    if (aDesc.hasWritable() && !aDesc.writable()) {
      return aResult.failInvalidDescriptor();
    }
    if (aDesc.hasValue()) {
      JS::Rooted<JSObject*> backingListObj(aCx);
      if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
        return false;
      }

      return SetLength(aCx, aProxy, backingListObj, aDesc.value(), aResult);
    }
    return aResult.succeed();
  }
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    if (aDesc.isAccessorDescriptor()) {
      return aResult.failNotDataDescriptor();
    }
    if (aDesc.hasConfigurable() && !aDesc.configurable()) {
      return aResult.failInvalidDescriptor();
    }
    if (aDesc.hasEnumerable() && !aDesc.enumerable()) {
      return aResult.failInvalidDescriptor();
    }
    if (aDesc.hasWritable() && !aDesc.writable()) {
      return aResult.failInvalidDescriptor();
    }
    if (aDesc.hasValue()) {
      JS::Rooted<JSObject*> backingListObj(aCx);
      if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
        return false;
      }

      return SetIndexedValue(aCx, aProxy, backingListObj, index, aDesc.value(),
                             aResult);
    }
    return aResult.succeed();
  }

  return ForwardingProxyHandler::defineProperty(aCx, aProxy, aId, aDesc,
                                                aResult);
}

bool ObservableArrayProxyHandler::delete_(JSContext* aCx,
                                          JS::Handle<JSObject*> aProxy,
                                          JS::Handle<JS::PropertyKey> aId,
                                          JS::ObjectOpResult& aResult) const {
  if (aId.get() == s_length_id) {
    return aResult.failCantDelete();
  }
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    JS::Rooted<JSObject*> backingListObj(aCx);
    if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
      return false;
    }

    uint32_t oldLen = 0;
    if (!JS::GetArrayLength(aCx, backingListObj, &oldLen)) {
      return false;
    }

    // We do not follow the spec (step 3.3 in
    // https://webidl.spec.whatwg.org/#es-observable-array-deleteProperty)
    // is because `oldLen - 1` could be `-1` if the backing list is empty, but
    // `oldLen` is `uint32_t` in practice. See also
    // https://github.com/whatwg/webidl/issues/1049.
    if (oldLen != index + 1) {
      return aResult.failBadIndex();
    }

    JS::Rooted<JS::Value> value(aCx);
    if (!JS_GetElement(aCx, backingListObj, index, &value)) {
      return false;
    }

    if (!OnDeleteItem(aCx, aProxy, value, index)) {
      return false;
    }

    if (!JS::SetArrayLength(aCx, backingListObj, index)) {
      return false;
    }

    return aResult.succeed();
  }
  return ForwardingProxyHandler::delete_(aCx, aProxy, aId, aResult);
}

bool ObservableArrayProxyHandler::get(JSContext* aCx,
                                      JS::Handle<JSObject*> aProxy,
                                      JS::Handle<JS::Value> aReceiver,
                                      JS::Handle<JS::PropertyKey> aId,
                                      JS::MutableHandle<JS::Value> aVp) const {
  JS::Rooted<JSObject*> backingListObj(aCx);
  if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
    return false;
  }

  uint32_t length = 0;
  if (!JS::GetArrayLength(aCx, backingListObj, &length)) {
    return false;
  }

  if (aId.get() == s_length_id) {
    return ToJSValue(aCx, length, aVp);
  }
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    if (index >= length) {
      aVp.setUndefined();
      return true;
    }
    return JS_GetElement(aCx, backingListObj, index, aVp);
  }
  return ForwardingProxyHandler::get(aCx, aProxy, aReceiver, aId, aVp);
}

bool ObservableArrayProxyHandler::getOwnPropertyDescriptor(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::Handle<JS::PropertyKey> aId,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) const {
  JS::Rooted<JSObject*> backingListObj(aCx);
  if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
    return false;
  }

  uint32_t length = 0;
  if (!JS::GetArrayLength(aCx, backingListObj, &length)) {
    return false;
  }

  if (aId.get() == s_length_id) {
    JS::Rooted<JS::Value> value(aCx, JS::NumberValue(length));
    aDesc.set(Some(JS::PropertyDescriptor::Data(
        value, {JS::PropertyAttribute::Writable})));
    return true;
  }
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    if (index >= length) {
      return true;
    }

    JS::Rooted<JS::Value> value(aCx);
    if (!JS_GetElement(aCx, backingListObj, index, &value)) {
      return false;
    }

    aDesc.set(Some(JS::PropertyDescriptor::Data(
        value,
        {JS::PropertyAttribute::Configurable, JS::PropertyAttribute::Writable,
         JS::PropertyAttribute::Enumerable})));
    return true;
  }
  return ForwardingProxyHandler::getOwnPropertyDescriptor(aCx, aProxy, aId,
                                                          aDesc);
}

bool ObservableArrayProxyHandler::has(JSContext* aCx,
                                      JS::Handle<JSObject*> aProxy,
                                      JS::Handle<JS::PropertyKey> aId,
                                      bool* aBp) const {
  if (aId.get() == s_length_id) {
    *aBp = true;
    return true;
  }
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    uint32_t length = 0;
    if (!GetBackingListLength(aCx, aProxy, &length)) {
      return false;
    }

    *aBp = (index < length);
    return true;
  }
  return ForwardingProxyHandler::has(aCx, aProxy, aId, aBp);
}

bool ObservableArrayProxyHandler::ownPropertyKeys(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandleVector<jsid> aProps) const {
  uint32_t length = 0;
  if (!GetBackingListLength(aCx, aProxy, &length)) {
    return false;
  }

  for (int32_t i = 0; i < int32_t(length); i++) {
    if (!aProps.append(JS::PropertyKey::Int(i))) {
      return false;
    }
  }
  return ForwardingProxyHandler::ownPropertyKeys(aCx, aProxy, aProps);
}

bool ObservableArrayProxyHandler::preventExtensions(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::ObjectOpResult& aResult) const {
  return aResult.failCantPreventExtensions();
}

bool ObservableArrayProxyHandler::set(JSContext* aCx,
                                      JS::Handle<JSObject*> aProxy,
                                      JS::Handle<JS::PropertyKey> aId,
                                      JS::Handle<JS::Value> aV,
                                      JS::Handle<JS::Value> aReceiver,
                                      JS::ObjectOpResult& aResult) const {
  if (aId.get() == s_length_id) {
    JS::Rooted<JSObject*> backingListObj(aCx);
    if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
      return false;
    }

    return SetLength(aCx, aProxy, backingListObj, aV, aResult);
  }
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    JS::Rooted<JSObject*> backingListObj(aCx);
    if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
      return false;
    }

    return SetIndexedValue(aCx, aProxy, backingListObj, index, aV, aResult);
  }
  return ForwardingProxyHandler::set(aCx, aProxy, aId, aV, aReceiver, aResult);
}

bool ObservableArrayProxyHandler::GetBackingListObject(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandle<JSObject*> aBackingListObject) const {
  // Retrieve the backing list object from the reserved slot on the proxy
  // object. If it doesn't exist yet, create it.
  JS::Rooted<JS::Value> slotValue(aCx);
  slotValue = js::GetProxyReservedSlot(
      aProxy, OBSERVABLE_ARRAY_BACKING_LIST_OBJECT_SLOT);
  if (slotValue.isUndefined()) {
    JS::Rooted<JSObject*> newBackingListObj(aCx);
    newBackingListObj.set(JS::NewArrayObject(aCx, 0));
    if (NS_WARN_IF(!newBackingListObj)) {
      return false;
    }
    slotValue = JS::ObjectValue(*newBackingListObj);
    js::SetProxyReservedSlot(aProxy, OBSERVABLE_ARRAY_BACKING_LIST_OBJECT_SLOT,
                             slotValue);
  }
  aBackingListObject.set(&slotValue.toObject());
  return true;
}

bool ObservableArrayProxyHandler::GetBackingListLength(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, uint32_t* aLength) const {
  JS::Rooted<JSObject*> backingListObj(aCx);
  if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
    return false;
  }

  return JS::GetArrayLength(aCx, backingListObj, aLength);
}

bool ObservableArrayProxyHandler::SetLength(JSContext* aCx,
                                            JS::Handle<JSObject*> aProxy,
                                            uint32_t aLength) const {
  JS::Rooted<JSObject*> backingListObj(aCx);
  if (!GetBackingListObject(aCx, aProxy, &backingListObj)) {
    return false;
  }

  JS::ObjectOpResult result;
  if (!SetLength(aCx, aProxy, backingListObj, aLength, result)) {
    return false;
  }

  return result ? true : result.reportError(aCx, aProxy);
}

bool ObservableArrayProxyHandler::SetLength(JSContext* aCx,
                                            JS::Handle<JSObject*> aProxy,
                                            JS::Handle<JSObject*> aBackingList,
                                            uint32_t aLength,
                                            JS::ObjectOpResult& aResult) const {
  uint32_t oldLen;
  if (!JS::GetArrayLength(aCx, aBackingList, &oldLen)) {
    return false;
  }

  if (aLength > oldLen) {
    return aResult.failBadArrayLength();
  }

  bool ok = true;
  uint32_t len = oldLen;
  for (; len > aLength; len--) {
    uint32_t indexToDelete = len - 1;
    JS::Rooted<JS::Value> value(aCx);
    if (!JS_GetElement(aCx, aBackingList, indexToDelete, &value)) {
      ok = false;
      break;
    }

    if (!OnDeleteItem(aCx, aProxy, value, indexToDelete)) {
      ok = false;
      break;
    }
  }

  return JS::SetArrayLength(aCx, aBackingList, len) && ok ? aResult.succeed()
                                                          : false;
}

bool ObservableArrayProxyHandler::SetLength(JSContext* aCx,
                                            JS::Handle<JSObject*> aProxy,
                                            JS::Handle<JSObject*> aBackingList,
                                            JS::Handle<JS::Value> aValue,
                                            JS::ObjectOpResult& aResult) const {
  uint32_t uint32Len;
  if (!ToUint32(aCx, aValue, &uint32Len)) {
    return false;
  }

  double numberLen;
  if (!ToNumber(aCx, aValue, &numberLen)) {
    return false;
  }

  if (uint32Len != numberLen) {
    JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                              JSMSG_BAD_INDEX);
    return false;
  }

  return SetLength(aCx, aProxy, aBackingList, uint32Len, aResult);
}

}  // namespace mozilla::dom
