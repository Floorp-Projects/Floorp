/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileReaderSync.h"

#include "nsIDOMFile.h"
#include "nsDOMError.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jsfriendapi.h"
#include "nsJSUtils.h"

#include "Exceptions.h"
#include "File.h"
#include "FileReaderSyncPrivate.h"
#include "WorkerInlines.h"

#define FUNCTION_FLAGS \
  JSPROP_ENUMERATE

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::exceptions::ThrowDOMExceptionForNSResult;

namespace {

inline bool
EnsureSucceededOrThrow(JSContext* aCx, nsresult rv)
{
  if (NS_SUCCEEDED(rv)) {
    return true;
  }

  rv = rv == NS_ERROR_FILE_NOT_FOUND ?
              NS_ERROR_DOM_FILE_NOT_FOUND_ERR :
              NS_ERROR_DOM_FILE_NOT_READABLE_ERR;
  ThrowDOMExceptionForNSResult(aCx, rv);
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
  Finalize(JSFreeOp* aFop, JSObject* aObj)
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

    JSObject* jsArrayBuffer = JS_NewArrayBuffer(aCx, blobSize);
    if (!jsArrayBuffer) {
      return false;
    }

    uint32_t bufferLength = JS_GetArrayBufferByteLength(jsArrayBuffer, aCx);
    uint8_t* arrayBuffer = JS_GetArrayBufferData(jsArrayBuffer, aCx);

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
