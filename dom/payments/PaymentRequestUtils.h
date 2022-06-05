/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestUtils_h
#define mozilla_dom_PaymentRequestUtils_h

#include "js/TypeDecls.h"
#include "nsTArray.h"

namespace mozilla::dom {

nsresult SerializeFromJSObject(JSContext* aCx, JS::Handle<JSObject*> aObject,
                               nsAString& aSerializedObject);

nsresult SerializeFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aValue,
                            nsAString& aSerializedValue);

nsresult DeserializeToJSObject(const nsAString& aSerializedObject,
                               JSContext* aCx,
                               JS::MutableHandle<JSObject*> aObject);

nsresult DeserializeToJSValue(const nsAString& aSerializedObject,
                              JSContext* aCx,
                              JS::MutableHandle<JS::Value> aValue);

}  // namespace mozilla::dom

#endif
