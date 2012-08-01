/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothUtils.h"
#include "jsapi.h"
#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/Scoped.h"

nsresult
mozilla::dom::bluetooth::StringArrayToJSArray(JSContext* aCx, JSObject* aGlobal,
                                              const nsTArray<nsString>& aSourceArray,
                                              JSObject** aResultArray)
{
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aGlobal, "Null global!");

  JSAutoRequest ar(aCx);
  JSAutoEnterCompartment ac;
  if (!ac.enter(aCx, aGlobal)) {
    NS_WARNING("Failed to enter compartment!");
    return NS_ERROR_FAILURE;
  }

  JSObject* arrayObj;

  if (aSourceArray.IsEmpty()) {
    arrayObj = JS_NewArrayObject(aCx, 0, nullptr);
  } else {
    uint32_t valLength = aSourceArray.Length();
    mozilla::ScopedDeleteArray<jsval> valArray(new jsval[valLength]);
    JS::AutoArrayRooter tvr(aCx, valLength, valArray);
    for (PRUint32 index = 0; index < valLength; index++) {
      JSString* s = JS_NewUCStringCopyN(aCx, aSourceArray[index].BeginReading(),
                                        aSourceArray[index].Length());
      if(!s) {
        NS_WARNING("Memory allocation error!");
        return NS_ERROR_OUT_OF_MEMORY;
      }
      valArray[index] = STRING_TO_JSVAL(s);
    }
    arrayObj = JS_NewArrayObject(aCx, valLength, valArray);
  }

  if (!arrayObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // XXX This is not what Jonas wants. He wants it to be live.
  // Followup at bug 717414
  if (!JS_FreezeObject(aCx, arrayObj)) {
    return NS_ERROR_FAILURE;
  }

  *aResultArray = arrayObj;
  return NS_OK;
}

