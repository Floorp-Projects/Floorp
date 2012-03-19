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

#include "FileReaderSync.h"

#include "nsIDOMFile.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jsfriendapi.h"
#include "jstypedarray.h"
#include "nsJSUtils.h"

#include "Exceptions.h"
#include "File.h"
#include "FileReaderSyncPrivate.h"
#include "WorkerInlines.h"

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::exceptions::ThrowFileExceptionForCode;
using js::ArrayBuffer;

namespace {

inline bool
EnsureSucceededOrThrow(JSContext* aCx, nsresult rv)
{
  if (NS_SUCCEEDED(rv)) {
    return true;
  }

  int code = rv == NS_ERROR_FILE_NOT_FOUND ?
              FILE_NOT_FOUND_ERR :
              FILE_NOT_READABLE_ERR;
  ThrowFileExceptionForCode(aCx, code);
  return false;
}

inline nsIDOMBlob*
GetDOMBlobFromJSObject(JSContext* aCx, JSObject* aObj) {
  // aObj can be null as JS_ConvertArguments("o") successfully converts JS
  // null to a null pointer to JSObject 
  if (aObj) {
    nsIDOMBlob* blob = file::GetDOMBlobFromJSObject(aObj);
    if (blob) {
      return blob;
    }
  }

  JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL, JSMSG_UNEXPECTED_TYPE,
                       aObj ? JS_GetClass(aObj)->name : "Object", "not a Blob.");
  return NULL;
}

class FileReaderSync
{
  // FileReaderSync should not be instantiated.
  FileReaderSync();
  ~FileReaderSync();

  static JSClass sClass;
  static JSFunctionSpec sFunctions[];

public:
  static JSObject*
  InitClass(JSContext* aCx, JSObject* aObj)
  {
    return JS_InitClass(aCx, aObj, NULL, &sClass, Construct, 0,
                        NULL, sFunctions, NULL, NULL);
  }

  static FileReaderSyncPrivate*
  GetPrivate(JSObject* aObj)
  {
    if (aObj) {
      JSClass* classPtr = JS_GetClass(aObj);
      if (classPtr == &sClass) {
        FileReaderSyncPrivate* fileReader =
          GetJSPrivateSafeish<FileReaderSyncPrivate>(aObj);
        return fileReader;
      }
    }
    return NULL;
  }

private:
  static FileReaderSyncPrivate*
  GetInstancePrivate(JSContext* aCx, JSObject* aObj, const char* aFunctionName)
  {
    FileReaderSyncPrivate* fileReader = GetPrivate(aObj);
    if (fileReader) {
      return fileReader;
    }

    JS_ReportErrorNumber(aCx, js_GetErrorMessage, NULL,
                         JSMSG_INCOMPATIBLE_PROTO, sClass.name, aFunctionName,
                         JS_GetClass(aObj)->name);
    return NULL;
  }

  static JSBool
  Construct(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_NewObject(aCx, &sClass, NULL, NULL);
    if (!obj) {
      return false;
    }

    FileReaderSyncPrivate* fileReader = new FileReaderSyncPrivate();
    NS_ADDREF(fileReader);

    SetJSPrivateSafeish(obj, fileReader);

    JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(obj));
    return true;
  }

  static void
  Finalize(JSContext* aCx, JSObject* aObj)
  {
    JS_ASSERT(JS_GetClass(aObj) == &sClass);
    FileReaderSyncPrivate* fileReader =
      GetJSPrivateSafeish<FileReaderSyncPrivate>(aObj);
    NS_IF_RELEASE(fileReader);
  }

  static JSBool
  ReadAsArrayBuffer(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    FileReaderSyncPrivate* fileReader =
      GetInstancePrivate(aCx, obj, "readAsArrayBuffer");
    if (!fileReader) {
      return false;
    }

    JSObject* jsBlob;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "o", &jsBlob)) {
      return false;
    }

    nsIDOMBlob* blob = GetDOMBlobFromJSObject(aCx, jsBlob);
    if (!blob) {
      return false;
    }

    PRUint64 blobSize;
    nsresult rv = blob->GetSize(&blobSize);
    if (!EnsureSucceededOrThrow(aCx, rv)) {
      return false;
    }

    JSObject* jsArrayBuffer = js_CreateArrayBuffer(aCx, blobSize);
    if (!jsArrayBuffer) {
      return false;
    }

    uint32_t bufferLength = JS_GetArrayBufferByteLength(jsArrayBuffer);
    uint8_t* arrayBuffer = JS_GetArrayBufferData(jsArrayBuffer);

    rv = fileReader->ReadAsArrayBuffer(blob, bufferLength, arrayBuffer);
    if (!EnsureSucceededOrThrow(aCx, rv)) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, OBJECT_TO_JSVAL(jsArrayBuffer));
    return true;
  }

  static JSBool
  ReadAsDataURL(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    FileReaderSyncPrivate* fileReader =
      GetInstancePrivate(aCx, obj, "readAsDataURL");
    if (!fileReader) {
      return false;
    }

    JSObject* jsBlob;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "o", &jsBlob)) {
      return false;
    }

    nsIDOMBlob* blob = GetDOMBlobFromJSObject(aCx, jsBlob);
    if (!blob) {
      return false;
    }

    nsString blobText;
    nsresult rv = fileReader->ReadAsDataURL(blob, blobText);
    if (!EnsureSucceededOrThrow(aCx, rv)) {
      return false;
    }

    JSString* jsBlobText = JS_NewUCStringCopyN(aCx, blobText.get(),
                                               blobText.Length());
    if (!jsBlobText) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, STRING_TO_JSVAL(jsBlobText));
    return true;
  }

  static JSBool
  ReadAsBinaryString(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    FileReaderSyncPrivate* fileReader =
      GetInstancePrivate(aCx, obj, "readAsBinaryString");
    if (!fileReader) {
      return false;
    }

    JSObject* jsBlob;
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "o", &jsBlob)) {
      return false;
    }

    nsIDOMBlob* blob = GetDOMBlobFromJSObject(aCx, jsBlob);
    if (!blob) {
      return false;
    }

    nsString blobText;
    nsresult rv = fileReader->ReadAsBinaryString(blob, blobText);
    if (!EnsureSucceededOrThrow(aCx, rv)) {
      return false;
    }

    JSString* jsBlobText = JS_NewUCStringCopyN(aCx, blobText.get(),
                                               blobText.Length());
    if (!jsBlobText) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, STRING_TO_JSVAL(jsBlobText));
    return true;
  }

  static JSBool
  ReadAsText(JSContext* aCx, unsigned aArgc, jsval* aVp)
  {
    JSObject* obj = JS_THIS_OBJECT(aCx, aVp);
    if (!obj) {
      return false;
    }

    FileReaderSyncPrivate* fileReader =
      GetInstancePrivate(aCx, obj, "readAsText");
    if (!fileReader) {
      return false;
    }

    JSObject* jsBlob;
    JSString* jsEncoding = JS_GetEmptyString(JS_GetRuntime(aCx));
    if (!JS_ConvertArguments(aCx, aArgc, JS_ARGV(aCx, aVp), "o/S", &jsBlob,
                             &jsEncoding)) {
      return false;
    }

    nsDependentJSString encoding;
    if (!encoding.init(aCx, jsEncoding)) {
      return false;
    }

    nsIDOMBlob* blob = GetDOMBlobFromJSObject(aCx, jsBlob);
    if (!blob) {
      return false;
    }

    nsString blobText;
    nsresult rv = fileReader->ReadAsText(blob, encoding, blobText);
    if (!EnsureSucceededOrThrow(aCx, rv)) {
      return false;
    }

    JSString* jsBlobText = JS_NewUCStringCopyN(aCx, blobText.get(),
                                               blobText.Length());
    if (!jsBlobText) {
      return false;
    }

    JS_SET_RVAL(aCx, aVp, STRING_TO_JSVAL(jsBlobText));
    return true;
  }
};

JSClass FileReaderSync::sClass = {
  "FileReaderSync",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Finalize
};

JSFunctionSpec FileReaderSync::sFunctions[] = {
  JS_FN("readAsArrayBuffer", ReadAsArrayBuffer, 1, FUNCTION_FLAGS),
  JS_FN("readAsBinaryString", ReadAsBinaryString, 1, FUNCTION_FLAGS),
  JS_FN("readAsText", ReadAsText, 1, FUNCTION_FLAGS),
  JS_FN("readAsDataURL", ReadAsDataURL, 1, FUNCTION_FLAGS),
  JS_FS_END
};

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

namespace filereadersync {

bool
InitClass(JSContext* aCx, JSObject* aGlobal)
{
  return !!FileReaderSync::InitClass(aCx, aGlobal);
}

} // namespace filereadersync

END_WORKERS_NAMESPACE
