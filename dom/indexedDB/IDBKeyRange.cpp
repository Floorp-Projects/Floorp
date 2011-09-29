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

#include "jscntxt.h"
#include "nsDOMClassInfo.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"

#include "Key.h"

USING_INDEXEDDB_NAMESPACE

namespace {

inline
JSBool
ConvertArguments(JSContext* aCx,
                 uintN aArgc,
                 jsval* aVp,
                 const char* aMethodName,
                 nsTArray<nsCOMPtr<nsIVariant> >& aKeys)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aCx, "Null pointer!");
  NS_ASSERTION(aVp, "Null pointer!");
  NS_ASSERTION(aMethodName, "Null pointer!");
  NS_ASSERTION(aKeys.Capacity(), "Need guaranteed capacity!");
  NS_ASSERTION(aKeys.IsEmpty(), "Not an empty array!");

  if (aArgc < aKeys.Capacity()) {
    nsCString num;
    num.AppendInt(aKeys.Length());
    JS_ReportErrorNumberUC(aCx, js_GetErrorMessage, nsnull,
                           JSMSG_MORE_ARGS_NEEDED, aMethodName, num.get(),
                           aKeys.Capacity() == 1 ? "" : "s");
    return JS_FALSE;
  }

  for (uintN i = 0; i < aKeys.Capacity(); i++) {
    jsval& arg = JS_ARGV(aCx, aVp)[i];
    if (JSVAL_IS_VOID(arg) || JSVAL_IS_NULL(arg) ||
        !Key::CanBeConstructedFromJSVal(arg)) {
      JS_ReportError(aCx, "Argument is not a supported key type.");
      return JS_FALSE;
    }

    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    NS_ASSERTION(xpc, "This should never be null!");

    nsCOMPtr<nsIVariant>* key = aKeys.AppendElement();
    NS_ASSERTION(key, "This should never fail!");

    if (NS_FAILED(xpc->JSValToVariant(aCx, &arg, getter_AddRefs(*key)))) {
      JS_ReportError(aCx, "Could not convert argument to variant.");
      return JS_FALSE;
    }
  }

  return JS_TRUE;
}

inline
JSBool
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

  JSObject* global = JS_GetGlobalForObject(aCx, JS_GetScopeChain(aCx));
  NS_ENSURE_TRUE(global, JS_FALSE);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
  if (NS_FAILED(xpc->WrapNative(aCx, global, aKeyRange,
                                NS_GET_IID(nsIIDBKeyRange),
                                getter_AddRefs(holder)))) {
    JS_ReportError(aCx, "Couldn't wrap IDBKeyRange object.");
    return JS_FALSE;
  }

  JSObject* result;
  if (NS_FAILED(holder->GetJSObject(&result))) {
    JS_ReportError(aCx, "Couldn't get JSObject from wrapper.");
    return JS_FALSE;
  }

  JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
MakeOnlyKeyRange(JSContext* aCx,
                 uintN aArgc,
                 jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsAutoTArray<nsCOMPtr<nsIVariant>, 1> keys;
  if (!ConvertArguments(aCx, aArgc, aVp, "IDBKeyRange.only", keys)) {
    return JS_FALSE;
  }
  NS_ASSERTION(keys.Length() == 1, "Didn't set all keys!");

  nsRefPtr<IDBKeyRange> range =
    IDBKeyRange::Create(keys[0], keys[0], PR_FALSE, PR_FALSE);
  NS_ASSERTION(range, "Out of memory?");

  if (!ReturnKeyRange(aCx, aVp, range)) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
MakeLowerBoundKeyRange(JSContext* aCx,
                       uintN aArgc,
                       jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsAutoTArray<nsCOMPtr<nsIVariant>, 1> keys;
  if (!ConvertArguments(aCx, aArgc, aVp, "IDBKeyRange.lowerBound", keys)) {
    return JS_FALSE;
  }
  NS_ASSERTION(keys.Length() == 1, "Didn't set all keys!");

  JSBool open = JS_FALSE;
  if (aArgc > 1 && !JS_ValueToBoolean(aCx, JS_ARGV(aCx, aVp)[1], &open)) {
    JS_ReportError(aCx, "Couldn't convert argument 2 to boolean.");
    return JS_FALSE;
  }

  nsRefPtr<IDBKeyRange> range =
    IDBKeyRange::Create(keys[0], nsnull, !!open, PR_TRUE);
  NS_ASSERTION(range, "Out of memory?");

  if (!ReturnKeyRange(aCx, aVp, range)) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
MakeUpperBoundKeyRange(JSContext* aCx,
                       uintN aArgc,
                       jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsAutoTArray<nsCOMPtr<nsIVariant>, 1> keys;
  if (!ConvertArguments(aCx, aArgc, aVp, "IDBKeyRange.upperBound", keys)) {
    return JS_FALSE;
  }
  NS_ASSERTION(keys.Length() == 1, "Didn't set all keys!");

  JSBool open = JS_FALSE;
  if (aArgc > 1 && !JS_ValueToBoolean(aCx, JS_ARGV(aCx, aVp)[1], &open)) {
    JS_ReportError(aCx, "Couldn't convert argument 2 to boolean.");
    return JS_FALSE;
  }

  nsRefPtr<IDBKeyRange> range =
    IDBKeyRange::Create(nsnull, keys[0], PR_TRUE, !!open);
  NS_ASSERTION(range, "Out of memory?");

  if (!ReturnKeyRange(aCx, aVp, range)) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

JSBool
MakeBoundKeyRange(JSContext* aCx,
                  uintN aArgc,
                  jsval* aVp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsAutoTArray<nsCOMPtr<nsIVariant>, 2> keys;
  if (!ConvertArguments(aCx, aArgc, aVp, "IDBKeyRange.bound", keys)) {
    return JS_FALSE;
  }
  NS_ASSERTION(keys.Length() == 2, "Didn't set all keys!");

  JSBool lowerOpen = JS_FALSE;
  if (aArgc > 2 && !JS_ValueToBoolean(aCx, JS_ARGV(aCx, aVp)[2], &lowerOpen)) {
    JS_ReportError(aCx, "Couldn't convert argument 3 to boolean.");
    return JS_FALSE;
  }

  JSBool upperOpen = JS_FALSE;
  if (aArgc > 3 && !JS_ValueToBoolean(aCx, JS_ARGV(aCx, aVp)[3], &upperOpen)) {
    JS_ReportError(aCx, "Couldn't convert argument 3 to boolean.");
    return JS_FALSE;
  }

  nsRefPtr<IDBKeyRange> range =
    IDBKeyRange::Create(keys[0], keys[1], lowerOpen, upperOpen);
  NS_ASSERTION(range, "Out of memory?");

  if (!ReturnKeyRange(aCx, aVp, range)) {
    return JS_FALSE;
  }

  return JS_TRUE;
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
already_AddRefed<IDBKeyRange>
IDBKeyRange::Create(nsIVariant* aLower,
                    nsIVariant* aUpper,
                    bool aLowerOpen,
                    bool aUpperOpen)
{
  nsRefPtr<IDBKeyRange> keyRange(new IDBKeyRange());
  keyRange->mLower = aLower;
  keyRange->mUpper = aUpper;
  keyRange->mLowerOpen = aLowerOpen;
  keyRange->mUpperOpen = aUpperOpen;

  return keyRange.forget();
}

NS_IMPL_ADDREF(IDBKeyRange)
NS_IMPL_RELEASE(IDBKeyRange)

NS_INTERFACE_MAP_BEGIN(IDBKeyRange)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBKeyRange)
  NS_INTERFACE_MAP_ENTRY(nsIIDBKeyRange)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBKeyRange)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBKeyRange, IDBKeyRange)

NS_IMETHODIMP
IDBKeyRange::GetLower(nsIVariant** aLower)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIVariant> result(mLower);
  result.forget(aLower);
  return NS_OK;
}

NS_IMETHODIMP
IDBKeyRange::GetUpper(nsIVariant** aUpper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIVariant> result(mUpper);
  result.forget(aUpper);
  return NS_OK;
}

NS_IMETHODIMP
IDBKeyRange::GetLowerOpen(bool* aLowerOpen)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aLowerOpen = mLowerOpen ? PR_TRUE : PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
IDBKeyRange::GetUpperOpen(bool* aUpperOpen)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aUpperOpen = mUpperOpen ? PR_TRUE : PR_FALSE;
  return NS_OK;
}
