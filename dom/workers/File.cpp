/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   William Chen <wchen@mozilla.com> (Original Author)
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

#include "File.h"

#include "nsIDOMFile.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jsfriendapi.h"
#include "nsCOMPtr.h"
#include "nsJSUtils.h"
#include "nsStringGlue.h"

#include "Exceptions.h"
#include "WorkerInlines.h"
#include "WorkerPrivate.h"

#define PROPERTY_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_SHARED)

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::exceptions::ThrowFileExceptionForCode;

namespace {

class Blob
{
  // Blob should never be instantiated.
  Blob();
  ~Blob();

  static JSClass sClass;
  static JSPropertySpec sProperties[];
  static JSFunctionSpec sFunctions[];

public:
  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj)
  {
    return JS_InitClass(aCx, aObj, NULL, &sClass, Construct, 0, sProperties,
                        sFunctions, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, nsIDOMBlob* aBlob)
  {
    JS_ASSERT(SameCOMIdentity(static_cast<nsISupports*>(aBlob), aBlob));

    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (obj) {
      JS_SetPrivate(obj, aBlob);
      NS_ADDREF(aBlob);
    }
    return obj;
  }

  static nsIDOMBlob*
  GetPrivate(JSObject* aObj);

private:
  static nsIDOMBlob*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    nsIDOMBlob* blob = GetPrivate(aObj);
    if (blob) {
      return blob;
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         JS_GetClass(aObj)->name);
    return NULL;
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                         sClass.name);
    return false;
  }

  static void
  Finalize(JSContext* aCx, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == &sClass);

    nsIDOMBlob* blob = GetPrivate(aObj);
    NS_IF_RELEASE(blob);
  }

  static JSBool
  GetSize(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    nsIDOMBlob* blob = GetInstancePrivate(aCx, aObj, "size");
    if (!blob) {
      return false;
    }

    PRUint64 size;
    if (NS_FAILED(blob->GetSize(&size))) {
      ThrowFileExceptionForCode(aCx, FILE_NOT_READABLE_ERR);
    }

    if (!JS_NewNumberValue(aCx, double(size), aVp)) {
      return false;
    }

    return true;
  }

  static JSBool
  GetType(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    nsIDOMBlob* blob = GetInstancePrivate(aCx, aObj, "type");
    if (!blob) {
      return false;
    }

    nsString type;
    if (NS_FAILED(blob->GetType(type))) {
      ThrowFileExceptionForCode(aCx, FILE_NOT_READABLE_ERR);
    }

    JSString* jsType = JS_NewUCStringCopyN(aCx, type.get(), type.Length());
    if (!jsType) {
      return false;
    }

    *aVp = STRING_TO_JSVAL(jsType);

    return true;
  }

  static JSBool
  Slice(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    nsIDOMBlob* blob = GetInstancePrivate(aCx, obj, "slice");
    if (!blob) {
      return false;
    }

    double start = 0, end = 0;
    JSString* jsContentType = JS_GetEmptyString(JS_GetRuntime(aCx));
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "/IIS", &start,
                             &end, &jsContentType)) {
      return false;
    }

    nsDependentJSString contentType;
    if (!contentType.init(aCx, jsContentType)) {
      return false;
    }

    PRUint8 optionalArgc = aArgc;
    nsCOMPtr<nsIDOMBlob> rtnBlob;
    if (NS_FAILED(blob->Slice(static_cast<PRUint64>(start),
                              static_cast<PRUint64>(end),
                              contentType, optionalArgc,
                              getter_AddRefs(rtnBlob)))) {
      ThrowFileExceptionForCode(aCx, FILE_NOT_READABLE_ERR);
      return false;
    }

    JSObject* rtnObj = file::CreateBlob(aCx, rtnBlob);
    if (!rtnObj) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(rtnObj));
    return true;
  }
};

JSClass Blob::sClass = {
  "Blob",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

JSPropertySpec Blob::sProperties[] = {
  { "size", 0, PROPERTY_FLAGS, GetSize, js_GetterOnlyPropertyStub },
  { "type", 0, PROPERTY_FLAGS, GetType, js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

JSFunctionSpec Blob::sFunctions[] = {
  JS_FN("slice", Slice, 1, JSPROP_ENUMERATE),
  JS_FS_END
};

class File : public Blob
{
  // File should never be instantiated.
  File();
  ~File();

  static JSClass sClass;
  static JSPropertySpec sProperties[];

public:
  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj, JSObject* aParentProto)
  {
    return JS_InitClass(aCx, aObj, aParentProto, &sClass, Construct, 0,
                        sProperties, NULL, NULL, NULL);
  }

  static JSObject*
  Create(JSContext* aCx, nsIDOMFile* aFile)
  {
    JS_ASSERT(SameCOMIdentity(static_cast<nsISupports*>(aFile), aFile));

    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (obj) {
      JS_SetPrivate(obj, aFile);
      NS_ADDREF(aFile);
    }
    return obj;
  }

  static nsIDOMFile*
  GetPrivate(JSObject* aObj)
  {
    if (aObj) {
      JSClass* classPtr = JS_GetClass(aObj);
      if (classPtr == &sClass) {
        nsISupports* priv = static_cast<nsISupports*>(JS_GetPrivate(aObj));
        nsCOMPtr<nsIDOMFile> file = do_QueryInterface(priv);
        JS_ASSERT_IF(priv, file);
        return file;
      }
    }
    return NULL;
  }

  static JSClass*
  Class()
  {
    return &sClass;
  }

private:
  static nsIDOMFile*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    nsIDOMFile* file = GetPrivate(aObj);
    if (file) {
      return file;
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         JS_GetClass(aObj)->name);
    return NULL;
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_WRONG_CONSTRUCTOR,
                         sClass.name);
    return false;
  }

  static void
  Finalize(JSContext* aCx, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == &sClass);

    nsIDOMFile* file = GetPrivate(aObj);
    NS_IF_RELEASE(file);
  }

  static JSBool
  GetMozFullPath(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    nsIDOMFile* file = GetInstancePrivate(aCx, aObj, "mozFullPath");
    if (!file) {
      return false;
    }

    nsString fullPath;

    if (GetWorkerPrivateFromContext(aCx)->UsesSystemPrincipal() &&
        NS_FAILED(file->GetMozFullPathInternal(fullPath))) {
      ThrowFileExceptionForCode(aCx, FILE_NOT_READABLE_ERR);
      return false;
    }

    JSString* jsFullPath = JS_NewUCStringCopyN(aCx, fullPath.get(),
                                               fullPath.Length());
    if (!jsFullPath) {
      return false;
    }

    *aVp = STRING_TO_JSVAL(jsFullPath);
    return true;
  }

  static JSBool
  GetName(JSContext* aCx, JSObject* aObj, jsid aIdval, jsval* aVp)
  {
    nsIDOMFile* file = GetInstancePrivate(aCx, aObj, "name");
    if (!file) {
      return false;
    }

    nsString name;
    if (NS_FAILED(file->GetName(name))) {
      name.Truncate();
    }

    JSString* jsName = JS_NewUCStringCopyN(aCx, name.get(), name.Length());
    if (!jsName) {
      return false;
    }

    *aVp = STRING_TO_JSVAL(jsName);
    return true;
  }
};

JSClass File::sClass = {
  "File",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

JSPropertySpec File::sProperties[] = {
  { "name", 0, PROPERTY_FLAGS, GetName, js_GetterOnlyPropertyStub },
  { "mozFullPath", 0, PROPERTY_FLAGS, GetMozFullPath,
    js_GetterOnlyPropertyStub },
  { 0, 0, 0, NULL, NULL }
};

nsIDOMBlob*
Blob::GetPrivate(JSObject* aObj)
{
  if (aObj) {
    JSClass* classPtr = JS_GetClass(aObj);
    if (classPtr == &sClass || classPtr == File::Class()) {
      nsISupports* priv = static_cast<nsISupports*>(JS_GetPrivate(aObj));
      nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(priv);
      JS_ASSERT_IF(priv, blob);
      return blob;
    }
  }
  return NULL;
}

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace file {

JSObject*
CreateBlob(JSContext* aCx, nsIDOMBlob* aBlob)
{
  return Blob::Create(aCx, aBlob);
}

bool
InitClasses(JSContext* aCx, JSObject* aGlobal)
{
  JSObject* blobProto = Blob::InitClass(aCx, aGlobal);
  return blobProto && File::InitClass(aCx, aGlobal, blobProto);
}

nsIDOMBlob*
GetDOMBlobFromJSObject(JSObject* aObj)
{
  return Blob::GetPrivate(aObj);
}

JSObject*
CreateFile(JSContext* aCx, nsIDOMFile* aFile)
{
  return File::Create(aCx, aFile);
}

nsIDOMFile*
GetDOMFileFromJSObject(JSObject* aObj)
{
  return File::GetPrivate(aObj);
}

} // namespace file

END_WORKERS_NAMESPACE
