/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#include "ChromeWorkerScope.h"

#include "jsapi.h"
#include "jscntxt.h"

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
    return nsnull;
  }

  char* result = static_cast<char*>(JS_malloc(aCx, native.Length() + 1));
  if (!result) {
    return nsnull;
  }

  memcpy(result, native.get(), native.Length());
  result[native.Length()] = 0;
  return result;
}

JSCTypesCallbacks gCTypesCallbacks = {
  UnicodeToNative
};

JSBool
CTypesLazyGetter(JSContext* aCx, JSObject* aObj, jsid aId, jsval* aVp)
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
  return JS_DeletePropertyById(aCx, aObj, aId) &&
         JS_InitCTypesClass(aCx, aObj) &&
         JS_GetPropertyById(aCx, aObj, aId, &ctypes) &&
         JS_SetCTypesCallbacks(aCx, JSVAL_TO_OBJECT(ctypes),
                               &gCTypesCallbacks) &&
         JS_GetPropertyById(aCx, aObj, aId, aVp);
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

namespace chromeworker {

bool
DefineChromeWorkerFunctions(JSContext* aCx, JSObject* aGlobal)
{
  // Currently ctypes is the only special property given to ChromeWorkers.
  return DefineCTypesLazyGetter(aCx, aGlobal);
}

} // namespace chromeworker

END_WORKERS_NAMESPACE
