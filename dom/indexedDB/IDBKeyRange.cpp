/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "IDBKeyRange.h"

#include "nsIXPConnect.h"

#include "nsDOMClassInfo.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "Key.h"

USING_INDEXEDDB_NAMESPACE

namespace {

inline
bool
ReturnKeyRange(JSContext* aCx,
               jsval* aVp,
               IDBKeyRange* aKeyRange)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null pointer!");
  NS_ASSERTION(aVp, "Null pointer!");
  NS_ASSERTION(aKeyRange, "Null pointer!");

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ASSERTION(xpc, "This should never be null!");

  JSObject* global = JS_GetGlobalForScopeChain(aCx);
  if (!global) {
    NS_WARNING("Couldn't get global object!");
    return false;
  }

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  if (NS_FAILED(xpc->WrapNative(aCx, global, aKeyRange,
                                NS_GET_IID(nsIIDBKeyRange),
                                getter_AddRefs(holder)))) {
    JS_ReportError(aCx, "Couldn't wrap IDBKeyRange object.");
    return false;
  }

  JSObject* result;
  if (NS_FAILED(holder->GetJSObject(&result))) {
    JS_ReportError(aCx, "Couldn't get JSObject from wrapper.");
    return false;
  }

  JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(result));
  return true;
}

inline
nsresult
GetKeyFromJSVal(JSContext* aCx,
                jsval aVal,
                Key& aKey,
                bool aAllowUnset = false)
{
  nsresult rv = aKey.SetFromJSVal(aCx, aVal);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM_INDEXEDDB,
                 "Bad error code!");
    return rv;
  }

  if (aKey.IsUnset() && !aAllowUnset) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  return NS_OK;
}

inline
void
ThrowException(JSContext* aCx,
               nsresult aErrorCode)
{
  NS_ASSERTION(NS_FAILED(aErrorCode), "Not an error code!");
  if (!JS_IsExceptionPending(aCx)) {
    nsDOMClassInfo::ThrowJSException(aCx, aErrorCode);
  }
}

inline
bool
GetKeyFromJSValOrThrow(JSContext* aCx,
                       jsval aVal,
                       Key& aKey)
{
  nsresult rv = GetKeyFromJSVal(aCx, aVal, aKey);
  if (NS_FAILED(rv)) {
    ThrowException(aCx, rv);
    return false;
  }
  return true;
}

JSBool
MakeOnlyKeyRange(JSContext* aCx,
                 unsigned aArgc,
                 jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  jsval val;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v", &val)) {
    return false;
  }

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(false, false, true);

  if (!GetKeyFromJSValOrThrow(aCx, val, keyRange->Lower())) {
    return false;
  }

  return ReturnKeyRange(aCx, aVp, keyRange);
}

JSBool
MakeLowerBoundKeyRange(JSContext* aCx,
                       unsigned aArgc,
                       jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  jsval val;
  JSBool open = false;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v/b", &val, &open)) {
    return false;
  }

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(open, true, false);

  if (!GetKeyFromJSValOrThrow(aCx, val, keyRange->Lower())) {
    return false;
  }

  return ReturnKeyRange(aCx, aVp, keyRange);
}

JSBool
MakeUpperBoundKeyRange(JSContext* aCx,
                       unsigned aArgc,
                       jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  jsval val;
  JSBool open = false;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "v/b", &val, &open)) {
    return false;
  }

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(true, open, false);

  if (!GetKeyFromJSValOrThrow(aCx, val, keyRange->Upper())) {
    return false;
  }

  return ReturnKeyRange(aCx, aVp, keyRange);
}

JSBool
MakeBoundKeyRange(JSContext* aCx,
                  unsigned aArgc,
                  jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  jsval lowerVal, upperVal;
  JSBool lowerOpen = false, upperOpen = false;
  if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "vv/bb", &lowerVal,
                           &upperVal, &lowerOpen, &upperOpen)) {
    return false;
  }

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(lowerOpen, upperOpen, false);

  if (!GetKeyFromJSValOrThrow(aCx, lowerVal, keyRange->Lower()) ||
      !GetKeyFromJSValOrThrow(aCx, upperVal, keyRange->Upper())) {
    return false;
  }

  if (keyRange->Lower() > keyRange->Upper() ||
      (keyRange->Lower() == keyRange->Upper() && (lowerOpen || upperOpen))) {
    ThrowException(aCx, NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return false;
  }

  return ReturnKeyRange(aCx, aVp, keyRange);
}

#define KEYRANGE_FUNCTION_FLAGS (JSPROP_ENUMERATE | JSPROP_PERMANENT)

const JSFunctionSpec gKeyRangeConstructors[] = {
  JS_FN("only", MakeOnlyKeyRange, 1, KEYRANGE_FUNCTION_FLAGS),
  JS_FN("lowerBound", MakeLowerBoundKeyRange, 1, KEYRANGE_FUNCTION_FLAGS),
  JS_FN("upperBound", MakeUpperBoundKeyRange, 1, KEYRANGE_FUNCTION_FLAGS),
  JS_FN("bound", MakeBoundKeyRange, 2, KEYRANGE_FUNCTION_FLAGS),
  JS_FS_END
};

#undef KEYRANGE_FUNCTION_FLAGS

} // anonymous namespace

// static
JSBool
IDBKeyRange::DefineConstructors(JSContext* aCx,
                                JSObject* aObject)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null pointer!");
  NS_ASSERTION(aObject, "Null pointer!");

  // Add the constructor methods for key ranges.
  return JS_DefineFunctions(aCx, aObject,
                            const_cast<JSFunctionSpec*>(gKeyRangeConstructors));
}

// static
nsresult
IDBKeyRange::FromJSVal(JSContext* aCx,
                       const jsval& aVal,
                       IDBKeyRange** aKeyRange)
{
  nsresult rv;
  nsRefPtr<IDBKeyRange> keyRange;

  if (JSVAL_IS_VOID(aVal) || JSVAL_IS_NULL(aVal)) {
    // undefined and null returns no IDBKeyRange.
  }
  else if (JSVAL_IS_PRIMITIVE(aVal) ||
           JS_IsArrayObject(aCx, JSVAL_TO_OBJECT(aVal)) ||
           JS_ObjectIsDate(aCx, JSVAL_TO_OBJECT(aVal))) {
    // A valid key returns an 'only' IDBKeyRange.
    keyRange = new IDBKeyRange(false, false, true);

    rv = GetKeyFromJSVal(aCx, aVal, keyRange->Lower());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else {
    // An object is not permitted unless it's another IDBKeyRange.
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    NS_ASSERTION(xpc, "This should never be null!");

    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
    rv = xpc->GetWrappedNativeOfJSObject(aCx, JSVAL_TO_OBJECT(aVal),
                                         getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    nsCOMPtr<nsIIDBKeyRange> iface;
    if (!wrapper || !(iface = do_QueryInterface(wrapper->Native()))) {
      // Some random JS object?
      return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
    }

    keyRange = static_cast<IDBKeyRange*>(iface.get());
  }

  keyRange.forget(aKeyRange);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBKeyRange)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBKeyRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBKeyRange)
  if (JSVAL_IS_GCTHING(tmp->mCachedLowerVal)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mCachedLowerVal);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing, "mCachedLowerVal")
  }
  if (JSVAL_IS_GCTHING(tmp->mCachedUpperVal)) {
    void *gcThing = JSVAL_TO_GCTHING(tmp->mCachedUpperVal);
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(gcThing, "mCachedUpperVal")
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBKeyRange)
  if (tmp->mRooted) {
    NS_DROP_JS_OBJECTS(tmp, IDBKeyRange);
    tmp->mCachedLowerVal = JSVAL_VOID;
    tmp->mCachedUpperVal = JSVAL_VOID;
    tmp->mHaveCachedLowerVal = false;
    tmp->mHaveCachedUpperVal = false;
    tmp->mRooted = false;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBKeyRange)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIIDBKeyRange)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBKeyRange)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBKeyRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBKeyRange)

DOMCI_DATA(IDBKeyRange, IDBKeyRange)

NS_IMETHODIMP
IDBKeyRange::GetLower(JSContext* aCx,
                      jsval* aLower)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveCachedLowerVal) {
    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, IDBKeyRange);
      mRooted = true;
    }

    nsresult rv = Lower().ToJSVal(aCx, &mCachedLowerVal);
    NS_ENSURE_SUCCESS(rv, rv);

    mHaveCachedLowerVal = true;
  }

  *aLower = mCachedLowerVal;
  return NS_OK;
}

NS_IMETHODIMP
IDBKeyRange::GetUpper(JSContext* aCx,
                      jsval* aUpper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveCachedUpperVal) {
    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, IDBKeyRange);
      mRooted = true;
    }

    nsresult rv = Upper().ToJSVal(aCx, &mCachedUpperVal);
    NS_ENSURE_SUCCESS(rv, rv);

    mHaveCachedUpperVal = true;
  }

  *aUpper = mCachedUpperVal;
  return NS_OK;
}

NS_IMETHODIMP
IDBKeyRange::GetLowerOpen(bool* aLowerOpen)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aLowerOpen = mLowerOpen;
  return NS_OK;
}


NS_IMETHODIMP
IDBKeyRange::GetUpperOpen(bool* aUpperOpen)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aUpperOpen = mUpperOpen;
  return NS_OK;
}
