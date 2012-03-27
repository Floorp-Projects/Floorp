/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSTARRAYHELPERS_H__
#define __NSTARRAYHELPERS_H__

template <class T>
inline nsresult
nsTArrayToJSArray(JSContext* aCx, const nsTArray<T>& aSourceArray,
                  JSObject** aResultArray)
{
  MOZ_ASSERT(aCx);
  JSAutoRequest ar(aCx);

  JSObject* arrayObj = JS_NewArrayObject(aCx, aSourceArray.Length(), nsnull);
  if (!arrayObj) {
    NS_WARNING("JS_NewArrayObject failed!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  MOZ_ASSERT(global);

  for (PRUint32 index = 0; index < aSourceArray.Length(); index++) {
    nsCOMPtr<nsISupports> obj;
    nsresult rv = CallQueryInterface(aSourceArray[index], getter_AddRefs(obj));
    NS_ENSURE_SUCCESS(rv, rv);

    jsval wrappedVal;
    rv = nsContentUtils::WrapNative(aCx, global, obj, &wrappedVal, nsnull, true);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_SetElement(aCx, arrayObj, index, &wrappedVal)) {
      NS_WARNING("JS_SetElement failed!");
      return NS_ERROR_FAILURE;
    }
  }

  if (!JS_FreezeObject(aCx, arrayObj)) {
    NS_WARNING("JS_FreezeObject failed!");
    return NS_ERROR_FAILURE;
  }

  *aResultArray = arrayObj;
  return NS_OK;
}

#endif /* __NSTARRAYHELPERS_H__ */
