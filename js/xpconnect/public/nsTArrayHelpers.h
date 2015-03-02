/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSTARRAYHELPERS_H__
#define __NSTARRAYHELPERS_H__

#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsTArray.h"

template <class T>
inline nsresult
nsTArrayToJSArray(JSContext* aCx, const nsTArray<T>& aSourceArray,
                  JS::MutableHandle<JSObject*> aResultArray)
{
  MOZ_ASSERT(aCx);

  JS::Rooted<JSObject*> arrayObj(aCx,
    JS_NewArrayObject(aCx, aSourceArray.Length()));
  if (!arrayObj) {
    NS_WARNING("JS_NewArrayObject failed!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t index = 0; index < aSourceArray.Length(); index++) {
    nsCOMPtr<nsISupports> obj;
    nsresult rv = aSourceArray[index]->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(obj));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::RootedValue wrappedVal(aCx);
    rv = nsContentUtils::WrapNative(aCx, obj, &wrappedVal);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_DefineElement(aCx, arrayObj, index, wrappedVal, JSPROP_ENUMERATE)) {
      NS_WARNING("JS_DefineElement failed!");
      return NS_ERROR_FAILURE;
    }
  }

  if (!JS_FreezeObject(aCx, arrayObj)) {
    NS_WARNING("JS_FreezeObject failed!");
    return NS_ERROR_FAILURE;
  }

  aResultArray.set(arrayObj);
  return NS_OK;
}

template <>
inline nsresult
nsTArrayToJSArray<nsString>(JSContext* aCx,
                            const nsTArray<nsString>& aSourceArray,
                            JS::MutableHandle<JSObject*> aResultArray)
{
  MOZ_ASSERT(aCx);

  JS::Rooted<JSObject*> arrayObj(aCx,
    JS_NewArrayObject(aCx, aSourceArray.Length()));
  if (!arrayObj) {
    NS_WARNING("JS_NewArrayObject failed!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JS::Rooted<JSString*> s(aCx);
  for (uint32_t index = 0; index < aSourceArray.Length(); index++) {
    s = JS_NewUCStringCopyN(aCx, aSourceArray[index].BeginReading(),
                            aSourceArray[index].Length());

    if(!s) {
      NS_WARNING("Memory allocation error!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_DefineElement(aCx, arrayObj, index, s, JSPROP_ENUMERATE)) {
      NS_WARNING("JS_DefineElement failed!");
      return NS_ERROR_FAILURE;
    }
  }

  if (!JS_FreezeObject(aCx, arrayObj)) {
    NS_WARNING("JS_FreezeObject failed!");
    return NS_ERROR_FAILURE;
  }

  aResultArray.set(arrayObj);
  return NS_OK;
}

#endif /* __NSTARRAYHELPERS_H__ */
