/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeWorkerScope.h"

#include "jsapi.h"

#include "nsXPCOM.h"
#include "nsNativeCharsetUtils.h"
#include "nsStringGlue.h"

#include "WorkerPrivate.h"

#define CTYPES_STR "ctypes"

USING_WORKERS_NAMESPACE

namespace {

#ifdef BUILD_CTYPES
char*
UnicodeToNative(JSContext* aCx, const jschar* aSource, size_t aSourceLen)
{
  nsDependentString unicode(aSource, aSourceLen);

  nsCAutoString native;
  if (NS_FAILED(NS_CopyUnicodeToNative(unicode, native))) {
    JS_ReportError(aCx, "Could not convert string to native charset!");
    return nullptr;
  }

  char* result = static_cast<char*>(JS_malloc(aCx, native.Length() + 1));
  if (!result) {
    return nullptr;
  }

  memcpy(result, native.get(), native.Length());
  result[native.Length()] = 0;
  return result;
}

JSCTypesCallbacks gCTypesCallbacks = {
  UnicodeToNative
};

JSBool
CTypesLazyGetter(JSContext* aCx, JSHandleObject aObj, JSHandleId aId, JSMutableHandleValue aVp)
{
  NS_ASSERTION(JS_GetGlobalObject(aCx) == aObj, "Not a global object!");
  NS_ASSERTION(JSID_IS_STRING(aId), "Bad id!");
  NS_ASSERTION(JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(aId), CTYPES_STR),
               "Bad id!");

  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  NS_ASSERTION(worker->IsChromeWorker(), "This should always be true!");

  if (!worker->DisableMemoryReporter()) {
    return false;
  }

  jsval ctypes;
  if (!JS_DeletePropertyById(aCx, aObj, aId) ||
      !JS_InitCTypesClass(aCx, aObj) ||
      !JS_GetPropertyById(aCx, aObj, aId, &ctypes)) {
    return false;
  }
  JS_SetCTypesCallbacks(JSVAL_TO_OBJECT(ctypes), &gCTypesCallbacks);
  return JS_GetPropertyById(aCx, aObj, aId, aVp.address());
}
#endif

inline bool
DefineCTypesLazyGetter(JSContext* aCx, JSObject* aGlobal)
{
#ifdef BUILD_CTYPES
  {
    JSString* ctypesStr = JS_InternString(aCx, CTYPES_STR);
    if (!ctypesStr) {
      return false;
    }

    jsid ctypesId = INTERNED_STRING_TO_JSID(aCx, ctypesStr);

    // We use a lazy getter here to let us unregister the blocking memory
    // reporter since ctypes can easily block the worker thread and we can
    // deadlock. Remove once bug 673323 is fixed.
    if (!JS_DefinePropertyById(aCx, aGlobal, ctypesId, JSVAL_VOID,
                               CTypesLazyGetter, NULL, 0)) {
      return false;
    }
  }
#endif

  return true;
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

bool
DefineChromeWorkerFunctions(JSContext* aCx, JSObject* aGlobal)
{
  // Currently ctypes is the only special property given to ChromeWorkers.
  return DefineCTypesLazyGetter(aCx, aGlobal);
}

END_WORKERS_NAMESPACE
